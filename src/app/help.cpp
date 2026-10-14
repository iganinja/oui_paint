#include "app/help.h"

#include "app/plugin_api.h"
#include "app/tools.h"

#include <oui/translation.h>

#include <algorithm>

namespace oui_paint
{

using olagarro::oui::format_text;
using olagarro::oui::translate;

std::vector<std::string_view> embedded_help();
std::vector<std::string_view> embedded_help_names();
std::vector<std::string_view> embedded_notices();

namespace
{

// A page of the help in the language installed, as "guide.es.md" for "guide.md", or in English when it has none.
std::string text_of(std::string_view name)
{
    const auto texts{embedded_help()};
    const auto names{embedded_help_names()};
    std::vector<std::string_view> variants;
    std::vector<std::string_view> variant_names;
    const auto dot{name.rfind('.')};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        const auto& candidate{names[index]};
        const auto same_stem{dot != std::string_view::npos && candidate.size() > name.size() &&
                             candidate.substr(0, dot + 1) == name.substr(0, dot + 1) &&
                             candidate.substr(candidate.size() - (name.size() - dot)) == name.substr(dot)};
        if(candidate == name)
        {
            variants.insert(variants.begin(), texts[index]);
            variant_names.insert(variant_names.begin(), candidate);
        }
        else if(same_stem)
        {
            variants.push_back(texts[index]);
            variant_names.push_back(candidate);
        }
    }
    if(variants.empty())
    {
        return {};
    }
    return std::string{olagarro::oui::text_for_language(variants, variant_names, olagarro::oui::installed_language())};
}

// The keys: the tools' letters, then every command bound to a key, under the menu it is in.
std::string keys_markdown(const olagarro::oui::CommandRegistry& commands)
{
    std::string markdown{"# " + translate("Help", "Keyboard") + "\n\n## " + translate("Help", "Tools") + "\n\n" +
                         translate("Help", "Pressing a tool's letter again goes to the next tool that shares it.") +
                         "\n\n| " + translate("Help", "Key") + " | " + translate("Help", "Tool") + " |\n|---|---|\n"};
    std::vector<std::pair<char32_t, std::string>> letters;
    for(const auto& tool : all_tools())
    {
        auto found{std::find_if(letters.begin(), letters.end(), [&](const auto& entry) { return entry.first == tool.key; })};
        if(found == letters.end())
        {
            letters.emplace_back(tool.key, tool.title);
        }
        else
        {
            found->second = format_text(translate("Help", "{0}, then {1}"), {found->second, tool.title});
        }
    }
    for(const auto& [key, names] : letters)
    {
        markdown += "| " + std::string(1, static_cast<char>(key - U'a' + U'A')) + " | " + names + " |\n";
    }
    // The menus by the first part of their commands' names, in the order the menus come.
    const std::vector<std::pair<std::string, std::vector<std::string>>> groups{
        {translate("Help", "File"), {"file."}},
        {translate("Help", "Edit"), {"edit."}},
        {translate("Help", "View and images"), {"view.", "window."}},
        {translate("Help", "Image"), {"image."}},
        {translate("Help", "Palettes"), {"palettes."}},
        {translate("Help", "Layers"), {"layers."}},
        {translate("Help", "Adjustments and effects"), {"effect.", "effects."}},
        {translate("Help", "Colors and brush"), {"colors.", "brush."}},
        {translate("Help", "Help"), {"help."}},
    };
    const auto listed{commands.commands()};
    for(const auto& [heading, prefixes] : groups)
    {
        std::string rows;
        for(const auto& command : listed)
        {
            const auto in_group{std::any_of(prefixes.begin(), prefixes.end(),
                                            [&](const std::string& prefix) { return command.id.rfind(prefix, 0) == 0; })};
            if(in_group && command.shortcut)
            {
                rows += "| " + olagarro::oui::format_shortcut(*command.shortcut) + " | " +
                        olagarro::oui::strip_mnemonic(command.title) + " |\n";
            }
        }
        if(!rows.empty())
        {
            markdown += "\n## " + heading + "\n\n| " + translate("Help", "Key") + " | " + translate("Help", "Command") +
                        " |\n|---|---|\n" + rows;
        }
    }
    return markdown;
}

} // namespace

std::string_view version()
{
    return OUI_PAINT_VERSION;
}

std::vector<HelpPage> help_pages(const olagarro::oui::CommandRegistry& commands)
{
    return {{translate("Help", "Guide"), "guide.md", text_of("guide.md")},
            {translate("Help", "Keyboard"), "keys.md", keys_markdown(commands)},
            {translate("Help", "Plugins"), "plugins.md", with_image_methods(text_of("plugins.md"))}};
}

std::optional<HelpPlace> help_place(const std::vector<HelpPage>& pages, std::string_view link)
{
    const auto hash{link.find('#')};
    const auto file{link.substr(0, hash)};
    for(std::size_t index{}; index < pages.size(); ++ index)
    {
        if(pages[index].file == file)
        {
            return HelpPlace{index, hash == std::string_view::npos ? std::string{} : std::string{link.substr(hash + 1)}};
        }
    }
    return std::nullopt;
}

std::string about_markdown()
{
    return "# OUI Paint " + std::string{version()} + "\n\n" +
           translate("Help", "A raster image editor in the spirit of Paint.NET, built on OUI.") + "\n\n" +
           translate("Help", "Free to use, change and pass on, under the zlib license:") + "\n\n```\n" + text_of("LICENSE") +
           "```\n\n" + translate("Help", "It carries other people's work, each under its own license: see Third-Party Notices.") +
           "\n";
}

std::string third_party_notices()
{
    // In the order the build embeds them.
    std::vector<std::string> names{"OUI (Olagarro)",
                                   "QuickJS-ng",
                                   "NanoSVG",
                                   "stb (stb_image, stb_image_write, stb_truetype)",
                                   "LodePNG",
                                   "miniz",
                                   "SDL3",
                                   "Spleen font",
                                   "Jersey 10 font",
                                   "Godot editor icons"};
#ifdef _WIN32
    names.insert(names.end(), {"mingw-w64 runtime", "winpthreads"});
#endif
    const auto texts{embedded_notices()};
    std::string markdown{"# " + translate("Help", "Third-party notices") + "\n\n" +
                         translate("Help", "OUI Paint carries these pieces, under these licenses.") + "\n"};
    for(std::size_t index{}; index < texts.size(); ++ index)
    {
        markdown += "\n## " + (index < names.size() ? names[index] : std::string{"?"}) + "\n\n```\n" +
                    std::string{texts[index]};
        if(!markdown.empty() && markdown.back() != '\n')
        {
            markdown += '\n';
        }
        markdown += "```\n";
    }
    return markdown;
}

} // namespace oui_paint

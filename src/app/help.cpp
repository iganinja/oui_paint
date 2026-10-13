#include "app/help.h"

#include "app/tools.h"

#include <algorithm>

namespace oui_paint
{

std::vector<std::string_view> embedded_help();
std::vector<std::string_view> embedded_help_names();
std::vector<std::string_view> embedded_notices();

namespace
{

std::string text_of(std::string_view name)
{
    const auto texts{embedded_help()};
    const auto names{embedded_help_names()};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        if(names[index] == name)
        {
            return std::string{texts[index]};
        }
    }
    return {};
}

// The keys: the tools' letters, then every command bound to a key, under the menu it is in.
std::string keys_markdown(const olagarro::oui::CommandRegistry& commands)
{
    std::string markdown{"# Keyboard\n\n## Tools\n\nPressing a tool's letter again goes to the next tool that shares "
                         "it.\n\n| Key | Tool |\n|---|---|\n"};
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
            found->second += ", then " + tool.title;
        }
    }
    for(const auto& [key, names] : letters)
    {
        markdown += "| " + std::string(1, static_cast<char>(key - U'a' + U'A')) + " | " + names + " |\n";
    }
    // The menus by the first part of their commands' names, in the order the menus come.
    const std::vector<std::pair<std::string, std::vector<std::string>>> groups{
        {"File", {"file."}},
        {"Edit", {"edit."}},
        {"View and images", {"view.", "window."}},
        {"Image", {"image."}},
        {"Palettes", {"palettes."}},
        {"Layers", {"layers."}},
        {"Adjustments and effects", {"effect.", "effects."}},
        {"Colors and brush", {"colors.", "brush."}},
        {"Help", {"help."}},
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
            markdown += "\n## " + heading + "\n\n| Key | Command |\n|---|---|\n" + rows;
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
    return {{"Guide", "guide.md", text_of("guide.md")},
            {"Keyboard", "keys.md", keys_markdown(commands)},
            {"Plugins", "plugins.md", text_of("plugins.md")}};
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
    return "# OUI Paint " + std::string{version()} +
           "\n\nA raster image editor in the spirit of Paint.NET, built on OUI.\n\n"
           "Free to use, change and pass on, under the zlib license:\n\n```\n" +
           text_of("LICENSE") + "```\n\nIt carries other people's work, each under its own license: see Third-Party "
                                "Notices.\n";
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
    std::string markdown{"# Third-party notices\n\nOUI Paint carries these pieces, under these licenses.\n"};
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

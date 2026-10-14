#include "app/fonts.h"

#include <oui/file_system.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <system_error>

namespace oui_paint
{

namespace
{

[[nodiscard]] std::string lower(std::string_view text)
{
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

[[nodiscard]] bool is_outline_font(const std::filesystem::path& path)
{
    const auto extension{lower(olagarro::oui::utf8_of(path.extension()))};
    return extension == ".ttf" || extension == ".otf" || extension == ".ttc";
}

// How much a style is the plain one: lower is plainer.
[[nodiscard]] int plainness(std::string_view style)
{
    const auto name{lower(style)};
    if(name == "regular" || name == "book" || name == "normal" || name == "roman")
    {
        return 0;
    }
    if(name == "medium")
    {
        return 1;
    }
    return 2 + static_cast<int>(name.size());
}

// Which of a family's faces a style is: bold, italic or both, as their usual names say; none for any other.
enum class Face
{
    other,
    bold,
    italic,
    bold_italic
};

[[nodiscard]] Face face_of(std::string_view style)
{
    const auto name{lower(style)};
    if(name == "bold")
    {
        return Face::bold;
    }
    if(name == "italic" || name == "oblique")
    {
        return Face::italic;
    }
    if(name == "bold italic" || name == "bold oblique" || name == "bolditalic" || name == "boldoblique")
    {
        return Face::bold_italic;
    }
    return Face::other;
}

[[nodiscard]] std::string first_of(std::string_view names)
{
    return std::string{names.substr(0, names.find(','))};
}

} // namespace

std::vector<FontFamily> parse_font_list(std::string_view listed)
{
    struct Found
    {
        std::filesystem::path path;
        int plainness{std::numeric_limits<int>::max()};
        std::filesystem::path bold;
        std::filesystem::path italic;
        std::filesystem::path bold_italic;
    };
    std::map<std::string, Found> families;
    std::size_t start{};
    while(start < listed.size())
    {
        auto end{listed.find('\n', start)};
        if(end == std::string_view::npos)
        {
            end = listed.size();
        }
        const auto line{listed.substr(start, end - start)};
        start = end + 1;
        const auto path_end{line.find(": ")};
        if(path_end == std::string_view::npos)
        {
            continue;
        }
        const auto path{olagarro::oui::path_from_utf8(line.substr(0, path_end))};
        if(!is_outline_font(path))
        {
            continue;
        }
        auto rest{line.substr(path_end + 2)};
        const auto style_at{rest.find(":style=")};
        const auto family{first_of(rest.substr(0, style_at))};
        const auto style{style_at == std::string_view::npos ? std::string{"Regular"}
                                                            : first_of(rest.substr(style_at + 7))};
        if(family.empty())
        {
            continue;
        }
        auto& found{families[family]};
        if(const auto how_plain{plainness(style)}; how_plain < found.plainness)
        {
            found.path = path;
            found.plainness = how_plain;
        }
        switch(face_of(style))
        {
        case Face::bold:
            found.bold = path;
            break;
        case Face::italic:
            found.italic = path;
            break;
        case Face::bold_italic:
            found.bold_italic = path;
            break;
        case Face::other:
            break;
        }
    }
    std::vector<FontFamily> result;
    for(const auto& [name, found] : families)
    {
        result.push_back({name, found.path, found.bold, found.italic, found.bold_italic});
    }
    return result;
}

std::vector<FontFamily> installed_fonts()
{
    if(auto* const pipe{popen("fc-list : family style file 2>/dev/null", "r")})
    {
        std::string listed;
        std::array<char, 65536> buffer{};
        while(const auto count{std::fread(buffer.data(), 1, buffer.size(), pipe)})
        {
            listed.append(buffer.data(), count);
        }
        if(pclose(pipe) == 0 && !listed.empty())
        {
            return parse_font_list(listed);
        }
    }
    std::vector<std::filesystem::path> folders{"/usr/share/fonts", "/usr/local/share/fonts", "C:/Windows/Fonts"};
    if(const auto* const home{std::getenv("HOME")})
    {
        folders.push_back(std::filesystem::path{home} / ".local/share/fonts");
        folders.push_back(std::filesystem::path{home} / ".fonts");
    }
    std::string listed;
    for(const auto& folder : folders)
    {
        std::error_code failure;
        for(std::filesystem::recursive_directory_iterator entry{folder, failure}, end; !failure && entry != end;
            entry.increment(failure))
        {
            if(entry->is_regular_file(failure) && is_outline_font(entry->path()))
            {
                listed += olagarro::oui::utf8_of(entry->path()) + ": " + olagarro::oui::utf8_of(entry->path().stem()) + "\n";
            }
        }
    }
    return parse_font_list(listed);
}

} // namespace oui_paint

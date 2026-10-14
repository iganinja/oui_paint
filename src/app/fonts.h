#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A family of type: the file of its regular face, and of its bold, italic and bold italic ones, empty where it has none.
struct FontFamily
{
    std::string name;
    std::filesystem::path path;
    std::filesystem::path bold{};
    std::filesystem::path italic{};
    std::filesystem::path bold_italic{};
};

// The families in what `fc-list : family style file` prints, one each, its regular face where it has one, or its
// plainest, and its bold, italic and bold italic ones, in alphabetical order; only TrueType and OpenType files.
[[nodiscard]] std::vector<FontFamily> parse_font_list(std::string_view listed);
// The families installed: through fc-list where there is one, otherwise the font files in the usual folders, named
// after their files.
[[nodiscard]] std::vector<FontFamily> installed_fonts();

} // namespace oui_paint

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A family of type and the file of its regular face.
struct FontFamily
{
    std::string name;
    std::filesystem::path path;
};

// The families in what `fc-list : family style file` prints, one each, its regular face where it has one, in
// alphabetical order; only TrueType and OpenType files.
[[nodiscard]] std::vector<FontFamily> parse_font_list(std::string_view listed);
// The families installed: through fc-list where there is one, otherwise the font files in the usual folders, named
// after their files.
[[nodiscard]] std::vector<FontFamily> installed_fonts();

} // namespace oui_paint

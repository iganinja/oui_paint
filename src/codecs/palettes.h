#pragma once

#include "core/document.h"
#include "core/palette.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A palette's colours from the bytes of a file named `name`: JASC .pal, GIMP .gpl, Photoshop .act, or an image, whose
// palette in its order when it is a PNG with one, otherwise its distinct colours in reading order, up to 256. None,
// with why in `error`, when it is none of them.
[[nodiscard]] std::optional<std::vector<Color>> read_palette_file(std::string_view name, std::string_view bytes,
                                                                  std::string& error);
// Colours as a palette file of the kind `name` ends in: .pal, .gpl or .act.
[[nodiscard]] std::string write_palette_file(std::string_view name, const std::vector<Color>& colors);

// A whole palette setup as JSON: its size, palettes with their names, colours and see-through index, and the colours
// allowed.
[[nodiscard]] std::string write_palette_setup(const PaletteSetup& setup);
[[nodiscard]] std::optional<PaletteSetup> read_palette_setup(std::string_view text, std::string& error);

// Whether the bytes are a PNG with a palette.
[[nodiscard]] bool is_paletted_png(std::string_view bytes);
// An indexed image as one PNG with a palette of all N times M entries in their order, palette p's entry i at
// p times M plus i, each pixel the topmost layer's that shows there. Throws when there are more than 256 entries.
[[nodiscard]] std::string write_indexed_png(const Document& document);
// A PNG with a palette as an indexed image of one palette of its entries, its indices as they are.
[[nodiscard]] std::optional<Document> read_indexed_png(std::string_view bytes, std::string& error);

} // namespace oui_paint

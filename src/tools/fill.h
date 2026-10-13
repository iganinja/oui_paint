#pragma once

#include "core/document.h"
#include "tools/stroke.h"

#include <vector>

namespace oui_paint
{

struct FillSettings
{
    // Every similar pixel of the image rather than those joined to the one clicked.
    bool global{false};
    // How different a pixel may be from the one clicked and still be filled, from 0 to 1, as the distance between
    // their channels, alpha among them.
    double tolerance{0.5};
    // Compares the pixels of the whole image as it shows rather than those of the active layer.
    bool sample_image{false};
    // Fills whole pixels only, as indices need.
    bool exact{false};
};

// Which pixels a fill from (x, y) reaches, as a coverage of 0 or 255 for each pixel of the image, row by row.
[[nodiscard]] std::vector<std::uint8_t> fill_region(const Document& document, int x, int y,
                                                    const FillSettings& settings);
// Fills from (x, y) on the active layer with `color`, inside the selection; returns the tiles written.
std::vector<TilePoint> bucket_fill(Document& document, int x, int y, const Color& color,
                                   const FillSettings& settings);

} // namespace oui_paint

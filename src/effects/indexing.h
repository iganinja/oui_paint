#pragma once

#include "core/color.h"
#include "effects/pieces.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace oui_paint
{

// The pieces that turn an image into indices of a palette, as the plugins that convert to indexed put them together.
// What they make is an image of indices: in each pixel, red is the index over 255 and alpha 1, or alpha 0 for a pixel
// see-through enough, half or more, to take the see-through index whatever the palette. Only the pixels that are not
// count for working out a palette.

// The image's different colours, as 8 bits a channel, and how many pixels have each, the commonest first; no more
// than `most` of them.
[[nodiscard]] std::vector<std::pair<Color, std::size_t>> distinct_colors(const WorkImage& image, std::size_t most);

// A palette of up to `count` colours for the image by median cut: the colours split again and again at the middle of
// the widest spread, each part's average one of the palette.
[[nodiscard]] std::vector<Color> median_cut(const WorkImage& image, int count);

// Each pixel the index of the palette's colour nearest to it.
[[nodiscard]] WorkImage nearest_indices(const WorkImage& image, const std::vector<Color>& palette);
// Each pixel the nearest colour's index, what that misses of the pixel's colour passed on to the pixels after it, as
// Floyd and Steinberg's dithering does; `strength` from 0, none passed on, to 1.
[[nodiscard]] WorkImage floyd_steinberg_indices(const WorkImage& image, const std::vector<Color>& palette,
                                                double strength);
// Each pixel the nearest colour's index after nudging it by a Bayer matrix of `size` 2, 4 or 8, an ordered dither's
// even pattern; `strength` from 0, none, to 1.
[[nodiscard]] WorkImage bayer_indices(const WorkImage& image, const std::vector<Color>& palette, int size,
                                      double strength);

// The pieces that change an image's format, as the plugins that convert it put them together.

// Each pixel grey, the weighted sum of its red, green and blue, the weights adding up to 1 or not.
[[nodiscard]] WorkImage grayscale(const WorkImage& image, double red, double green, double blue);
// Each channel brought to `levels` even steps from 0 to 1, as a format of fewer bits keeps them, nudged first by a
// Bayer matrix of `size` 2, 4 or 8, `strength` from 0, plain rounding, to 1, so bands of one step become a fine
// pattern.
[[nodiscard]] WorkImage dither_levels(const WorkImage& image, int levels, int size, double strength);

} // namespace oui_paint

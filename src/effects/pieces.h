#pragma once

#include "core/image.h"
#include "core/layer.h"

#include <array>
#include <cstdint>
#include <vector>

namespace oui_paint
{

// An image to work on: four numbers per pixel, red, green, blue and alpha, from 0 to 1, colour multiplied by alpha so
// mixing see-through pixels leaves no fringe, whatever the depth it came from.
struct WorkImage
{
    int width{0};
    int height{0};
    std::vector<float> pixels;

    WorkImage() = default;
    WorkImage(int width, int height) : width{width}, height{height}, pixels(static_cast<std::size_t>(width) * height * 4u) {}

    [[nodiscard]] float* at(int x, int y) { return &pixels[(static_cast<std::size_t>(y) * width + x) * 4u]; }
    [[nodiscard]] const float* at(int x, int y) const { return &pixels[(static_cast<std::size_t>(y) * width + x) * 4u]; }
    // The pixel at a place, the nearest edge pixel past the edges.
    [[nodiscard]] const float* clamped(int x, int y) const;
};

[[nodiscard]] WorkImage to_work(const Image& image);
// Back to `format`, colour divided by alpha again; grey from the colours' luma.
[[nodiscard]] Image from_work(const WorkImage& work, PixelFormat format);

// The pieces effects are made of. Each takes an image and gives a new one.
[[nodiscard]] WorkImage gaussian_blur(const WorkImage& image, double radius);
[[nodiscard]] WorkImage box_blur(const WorkImage& image, int radius);
// Averaged along a line through each pixel, `distance` pixels long at `degrees`, centred on it.
[[nodiscard]] WorkImage motion_blur(const WorkImage& image, double degrees, double distance);
// Averaged along an arc round (center_x, center_y), `degrees` of it.
[[nodiscard]] WorkImage radial_blur(const WorkImage& image, double degrees, double center_x, double center_y);
// Averaged along the line to (center_x, center_y), `amount` from 0 to 1 of the way.
[[nodiscard]] WorkImage zoom_blur(const WorkImage& image, double amount, double center_x, double center_y);
// Each pixel the sum of its neighbours times the matrix's numbers, `size` a side, divided and offset; alpha kept.
[[nodiscard]] WorkImage convolve(const WorkImage& image, const std::vector<double>& matrix, int size, double divisor,
                                 double offset);
// Fractal noise of grey, `scale` pixels to its largest features, `roughness` from 0 to 1 how much the finer ones count,
// the same for the same seed.
[[nodiscard]] WorkImage clouds(int width, int height, double scale, double roughness, std::uint32_t seed);
// Each colour channel through a table of 256 entries from 0 to 1, interpolated between them: curves, levels, invert,
// posterize. Alpha kept.
[[nodiscard]] WorkImage lookup(const WorkImage& image, const std::array<std::vector<double>, 3>& tables);
// Each pixel's luminosity through a table of values from 0 to 1, interpolated, its colour scaled with it so its hue
// stays, as the Curves adjustment's luminosity map does; a colour that would go past white goes towards it.
[[nodiscard]] WorkImage lookup_luminosity(const WorkImage& image, const std::vector<double>& table);
// Hue turned by `hue` degrees, saturation and lightness changed by -1 to 1.
[[nodiscard]] WorkImage hue_saturation(const WorkImage& image, double hue, double saturation, double lightness);
// `above` over `below` as a blend mode mixes them, `above` seen through by `opacity`.
[[nodiscard]] WorkImage mix(const WorkImage& below, const WorkImage& above, BlendMode mode, double opacity);

} // namespace oui_paint

#include "effects/test_card.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace oui_paint
{

namespace
{

[[nodiscard]] std::uint8_t byte(double value)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

// Red, green and blue of a hue from 0 to 1 at full saturation and value.
void hue_color(double hue, double& red, double& green, double& blue)
{
    const auto channel{[hue](double offset)
                       { return std::clamp(std::abs(std::fmod(hue * 6.0 + offset, 6.0) - 3.0) - 1.0, 0.0, 1.0); }};
    red = channel(0.0);
    green = channel(4.0);
    blue = channel(2.0);
}

} // namespace

Image test_card(int width, int height)
{
    Image image{PixelFormat::rgba8, width, height};
    const auto put{[&image](int x, int y, double red, double green, double blue, double alpha = 1.0)
                   { image.set_pixel(x, y, Rgba8{{byte(red), byte(green), byte(blue), byte(alpha)}}); }};
    // Bands from the top: bars, a gray ramp, hues, then a row of patches.
    const auto bars_end{height / 4};
    const auto ramp_end{bars_end + height / 8};
    const auto hues_end{ramp_end + height / 4};
    static constexpr double bars[8][3]{{1, 1, 1}, {1, 1, 0}, {0, 1, 1}, {0, 1, 0},
                                       {1, 0, 1}, {1, 0, 0}, {0, 0, 1}, {0, 0, 0}};
    std::uint32_t seed{12345};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            const auto across{width > 1 ? static_cast<double>(x) / (width - 1) : 0.0};
            if(y < bars_end)
            {
                const auto& bar{bars[std::min(7, x * 8 / width)]};
                put(x, y, bar[0], bar[1], bar[2]);
            }
            else if(y < ramp_end)
            {
                put(x, y, across, across, across);
            }
            else if(y < hues_end)
            {
                // Full hues at the top of the band, paler down to white.
                const auto pale{static_cast<double>(y - ramp_end) / std::max(1, hues_end - ramp_end - 1)};
                double red{};
                double green{};
                double blue{};
                hue_color(across, red, green, blue);
                put(x, y, red + (1.0 - red) * pale, green + (1.0 - green) * pale, blue + (1.0 - blue) * pale);
            }
            else
            {
                // Four patches: lines, a circle, noise, and see-through.
                const auto patch{std::min(3, x * 4 / width)};
                const auto left{patch * width / 4};
                const auto local_x{x - left};
                const auto local_y{y - hues_end};
                const auto patch_width{(patch + 1) * width / 4 - left};
                const auto patch_height{height - hues_end};
                if(patch == 0)
                {
                    // Lines one pixel apart, upright on the left half and across on the right.
                    const auto on{local_x < patch_width / 2 ? local_x % 2 == 0 : local_y % 2 == 0};
                    put(x, y, on ? 0.0 : 1.0, on ? 0.0 : 1.0, on ? 0.0 : 1.0);
                }
                else if(patch == 1)
                {
                    const auto radius{std::min(patch_width, patch_height) * 0.4};
                    const auto distance{std::hypot(local_x + 0.5 - patch_width / 2.0, local_y + 0.5 - patch_height / 2.0)};
                    const auto inside{std::clamp(radius - distance + 0.5, 0.0, 1.0)};
                    put(x, y, 1.0 - inside * 0.1, 1.0 - inside * 0.6, 1.0 - inside);
                }
                else if(patch == 2)
                {
                    seed = seed * 1664525u + 1013904223u;
                    const auto value{static_cast<double>(seed >> 24) / 255.0};
                    put(x, y, value, value * 0.8, value * 0.6);
                }
                else
                {
                    const auto alpha{patch_width > 1 ? static_cast<double>(local_x) / (patch_width - 1) : 1.0};
                    put(x, y, 0.1, 0.4, 0.9, alpha);
                }
            }
        }
    }
    return image;
}

} // namespace oui_paint

#pragma once

#include "core/layer.h"

#include <algorithm>
#include <cmath>

namespace oui_paint
{

// `above` laid over `below` as `mode` mixes them, `above` seen through by `opacity`, from 0 to 1. Colours are straight,
// not multiplied by their alpha, and so is the result.
template <typename PixelType>
[[nodiscard]] PixelType blend(const PixelType& below, const PixelType& above, double opacity, BlendMode mode)
{
    static_cast<void>(mode);
    constexpr double full{PixelType::full};
    constexpr int colors{PixelType::channels - 1};
    const auto above_alpha{above.alpha() / full * opacity};
    const auto below_alpha{below.alpha() / full};
    const auto alpha{above_alpha + below_alpha * (1.0 - above_alpha)};
    PixelType result;
    if(alpha <= 0.0)
    {
        return result;
    }
    for(int index{}; index < colors; ++ index)
    {
        const auto color{(above.value[index] * above_alpha + below.value[index] * below_alpha * (1.0 - above_alpha)) /
                         alpha};
        result.value[index] = static_cast<typename PixelType::ChannelType>(std::lround(std::clamp(color, 0.0, full)));
    }
    result.value[colors] = static_cast<typename PixelType::ChannelType>(std::lround(alpha * full));
    return result;
}

} // namespace oui_paint

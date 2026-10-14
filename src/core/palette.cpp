#include "core/palette.h"

#include <oui/translation.h>

#include <cmath>

namespace oui_paint
{

namespace
{

[[nodiscard]] double distance(const Color& a, const Color& b)
{
    // Weighted as eyes see, green most.
    const auto red{a.red - b.red};
    const auto green{a.green - b.green};
    const auto blue{a.blue - b.blue};
    return 0.3 * red * red + 0.59 * green * green + 0.11 * blue * blue;
}

} // namespace

PaletteSetup PaletteSetup::greys(int count, int size)
{
    PaletteSetup setup;
    setup.size = size;
    for(int slot{}; slot < count; ++ slot)
    {
        Palette palette{olagarro::oui::format_text(olagarro::oui::translate("Dialogs", "Palette {0}"), {std::to_string(slot + 1)}), {}, 0};
        for(int index{}; index < size; ++ index)
        {
            const auto value{size > 1 ? static_cast<double>(index) / (size - 1) : 0.0};
            palette.colors.push_back({value, value, value, 1.0});
        }
        setup.palettes.push_back(std::move(palette));
    }
    return setup;
}

Color PaletteSetup::color(int slot, int index) const
{
    if(slot < 0 || slot >= count())
    {
        return {0, 0, 0, 0};
    }
    const auto& palette{palettes[static_cast<std::size_t>(slot)]};
    if(index < 0 || index >= static_cast<int>(palette.colors.size()) || palette.transparent == index)
    {
        return {0, 0, 0, 0};
    }
    return palette.colors[static_cast<std::size_t>(index)];
}

int PaletteSetup::nearest(int slot, const Color& color) const
{
    if(slot < 0 || slot >= count())
    {
        return 0;
    }
    const auto& palette{palettes[static_cast<std::size_t>(slot)]};
    if(color.alpha < 0.5 && palette.transparent)
    {
        return *palette.transparent;
    }
    auto best{0};
    auto best_distance{1e18};
    for(int index{}; index < static_cast<int>(palette.colors.size()); ++ index)
    {
        if(palette.transparent == index)
        {
            continue;
        }
        const auto how_far{distance(palette.colors[static_cast<std::size_t>(index)], color)};
        if(how_far < best_distance)
        {
            best_distance = how_far;
            best = index;
        }
    }
    return best;
}

Color PaletteSetup::nearest_allowed(const Color& color) const
{
    if(allowed.empty())
    {
        return color;
    }
    const Color* best{&allowed.front()};
    for(const auto& candidate : allowed)
    {
        if(distance(candidate, color) < distance(*best, color))
        {
            best = &candidate;
        }
    }
    return {best->red, best->green, best->blue, color.alpha};
}

} // namespace oui_paint

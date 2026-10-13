#pragma once

#include "core/color.h"

#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// One palette: its colours, one of which may be see-through, as console graphics have index 0.
struct Palette
{
    std::string name;
    std::vector<Color> colors;
    std::optional<int> transparent{0};

    [[nodiscard]] bool operator==(const Palette& other) const
    {
        return name == other.name && colors == other.colors && transparent == other.transparent;
    }
    [[nodiscard]] bool operator!=(const Palette& other) const { return !(*this == other); }
};

// The palettes of an indexed image: `count` of them, each of `size` colours, and, when set, the only colours they may
// take, as a console's whole range.
struct PaletteSetup
{
    int size{16};
    std::vector<Palette> palettes;
    std::vector<Color> allowed;

    [[nodiscard]] int count() const { return static_cast<int>(palettes.size()); }
    [[nodiscard]] bool operator==(const PaletteSetup& other) const
    {
        return size == other.size && palettes == other.palettes && allowed == other.allowed;
    }
    [[nodiscard]] bool operator!=(const PaletteSetup& other) const { return !(*this == other); }
    // `count` palettes of `size` greys from black to white, index 0 see-through.
    [[nodiscard]] static PaletteSetup greys(int count, int size);
    // The colour of index `index` of palette `slot`; see-through for the transparent index and past the palette.
    [[nodiscard]] Color color(int slot, int index) const;
    // The index of palette `slot` nearest to `color`, never its see-through one; the see-through one for a colour
    // mostly see-through.
    [[nodiscard]] int nearest(int slot, const Color& color) const;
    // The allowed colour nearest to `color`; `color` itself when none are set.
    [[nodiscard]] Color nearest_allowed(const Color& color) const;
};

} // namespace oui_paint

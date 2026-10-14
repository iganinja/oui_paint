#pragma once

#include "core/layer.h"
#include "core/selection.h"
#include "render/viewport.h"
#include "tools/stroke.h"

#include <oui/graphics/font.h>

#include <string_view>
#include <vector>

namespace oui_paint
{

// What goes around text, each off at 0: an outline `outline` pixels wide; a shadow `shadow` pixels down and to the
// right, its edge soft; a glow reaching `glow` pixels out from the text and its outline, fading as it goes. The
// outline and the glow are in `color`, the shadow in `shadow_color` at `shadow_opacity`.
struct TextEffects
{
    int outline{0};
    int shadow{0};
    int glow{0};
    Color color{1, 1, 1, 1};
    Color shadow_color{0, 0, 0, 1};
    double shadow_opacity{0.6};

    [[nodiscard]] bool any() const { return outline > 0 || shadow > 0 || glow > 0; }
};

// Text in `font`, its lines split at '\n', with the top left of its first line at `at`, mixed over the layer inside the
// selection, over its shadow, glow and outline when `effects` has them; without antialiasing, each pixel covered half
// or more is solid. Returns the tiles written.
std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias,
                                 const TextEffects& effects = {});

} // namespace oui_paint

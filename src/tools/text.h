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

// What goes around text, each off at 0, each in its own colour: an outline `outline` pixels wide; a shadow `shadow`
// pixels down and to the right, its edge soft, as see-through as its colour's alpha makes it; a glow reaching `glow`
// pixels out from the text and its outline, fading as it goes.
struct TextEffects
{
    int outline{0};
    int shadow{0};
    int glow{0};
    Color outline_color{1, 1, 1, 1};
    Color shadow_color{0, 0, 0, 0.6};
    Color glow_color{1, 1, 1, 1};

    [[nodiscard]] bool any() const { return outline > 0 || shadow > 0 || glow > 0; }
};

// How the letters are made where the font has no face of its own for it: thicker, as bold, or leaning right, as
// italic; and whether a line goes through them, as struck through.
struct TextFace
{
    bool embolden{false};
    bool slant{false};
    bool strikethrough{false};
};

// Text in `font`, its lines split at '\n', with the top left of its first line at `at`, mixed over the layer inside the
// selection, as `face` makes its letters, over its shadow, glow and outline when `effects` has them; without
// antialiasing, each pixel covered half or more is solid. Returns the tiles written.
std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias,
                                 const TextEffects& effects = {}, const TextFace& face = {});

} // namespace oui_paint

#pragma once

#include "core/layer.h"
#include "core/selection.h"
#include "render/viewport.h"
#include "tools/stroke.h"

#include <oui/graphics/font.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// What goes around text, each off at 0, each in its own colour: an outline `outline` pixels wide; a shadow `shadow`
// pixels away from the light, its edge soft, as see-through as its colour's alpha makes it; a glow reaching `glow`
// pixels out from the text and its outline, fading as it goes. The light comes from `light` degrees, counted
// anticlockwise from the right, as the image shows it: 135, from the top left, casts the shadow down and to the right.
struct TextEffects
{
    int outline{0};
    int shadow{0};
    int glow{0};
    Color outline_color{1, 1, 1, 1};
    Color shadow_color{0, 0, 0, 0.6};
    Color glow_color{1, 1, 1, 1};
    double light{135.0};

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
// selection, as `face` makes its letters, turned `radians` clockwise round `at`, over its shadow, glow and outline when
// `effects` has them; without antialiasing, each pixel covered half or more is solid. Returns the tiles written.
std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias,
                                 const TextEffects& effects = {}, const TextFace& face = {}, double radians = 0.0);

// Text being typed, and where in it what is typed goes: a place between characters, as bytes from its start.
struct TypedText
{
    std::string text;
    std::size_t cursor{0};

    // What is typed, or a new line, at the cursor, which goes after it.
    void insert(std::string_view typed);
    // The character before the cursor, or after it; nothing at the start, or the end.
    void erase_back();
    void erase_forward();
    // One character to the left or the right; to the start or the end of its line.
    void left();
    void right();
    void home();
    void end();
    // To the line above or below, as near as it gets to where it was across, as `font` lays the lines out; nothing on
    // the first line, or the last.
    void up(olagarro::oui::Font& font);
    void down(olagarro::oui::Font& font);
    // Which line the cursor is on, from 0, and how far across it is, as `font` lays it out.
    [[nodiscard]] std::size_t line() const;
    [[nodiscard]] double across(olagarro::oui::Font& font) const;
};

// The corners of the box the lines of `text` take, from where it starts, as draw_text lays them out and turns them:
// its top left, top right, bottom right and bottom left.
[[nodiscard]] std::array<Position, 4> text_corners(olagarro::oui::Font& font, std::string_view text, Position at,
                                                   double radians);

} // namespace oui_paint

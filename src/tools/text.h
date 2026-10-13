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

// Text in `font`, its lines split at '\n', with the top left of its first line at `at`, mixed over the layer inside the
// selection; without antialiasing, each pixel the glyph covers half or more is solid. Returns the tiles written.
std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias);

} // namespace oui_paint

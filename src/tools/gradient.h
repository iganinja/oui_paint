#pragma once

#include "core/document.h"
#include "render/viewport.h"
#include "tools/stroke.h"

#include <string_view>
#include <vector>

namespace oui_paint
{

// Paint.NET's gradients.
enum class GradientKind
{
    linear,
    linear_reflected,
    diamond,
    radial,
    conical,
};

[[nodiscard]] const std::vector<GradientKind>& all_gradient_kinds();
[[nodiscard]] std::string_view gradient_name(GradientKind kind);

// How far along a gradient from `start` to `end` a point is, from 0 to 1.
[[nodiscard]] double gradient_position(GradientKind kind, Position start, Position end, Position point);

// Draws a gradient from `start` to `end` over the active layer of `base`, inside the selection: from `from` to `to`, or
// with `transparency`, the layer's own pixels made see-through from solid at the start to clear at the end. `base` is
// the layer as it was before, so dragging draws it anew each time. Returns the tiles written.
std::vector<TilePoint> draw_gradient(Layer& layer, const Layer& base, const Selection& selection, GradientKind kind,
                                     Position start, Position end, const Color& from, const Color& to,
                                     bool transparency);
// The same between two indices of an indexed layer, each pixel the one or the other by an ordered dither, as consoles
// draw gradients: `from` and `to` are the pixels themselves.
std::vector<TilePoint> draw_dithered_gradient(Layer& layer, const Layer& base, const Selection& selection,
                                              GradientKind kind, Position start, Position end, GrayAlpha8 from,
                                              GrayAlpha8 to);

} // namespace oui_paint

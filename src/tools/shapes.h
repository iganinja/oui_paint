#pragma once

#include "core/layer.h"
#include "core/selection.h"
#include "render/viewport.h"
#include "tools/stroke.h"

#include <array>
#include <string_view>
#include <vector>

namespace oui_paint
{

enum class ShapeKind
{
    rectangle,
    rounded_rectangle,
    ellipse,
};

// Paint.NET's: the outline in the primary colour, filled with the primary colour, or both, the inside then in the
// secondary colour.
enum class ShapeStyle
{
    outline,
    filled,
    outline_and_fill,
};

[[nodiscard]] std::string_view shape_name(ShapeKind kind);
[[nodiscard]] std::string_view shape_style_name(ShapeStyle style);

struct ShapeSettings
{
    ShapeKind kind{ShapeKind::rectangle};
    ShapeStyle style{ShapeStyle::outline};
};

struct LineSettings
{
    bool arrow_at_start{false};
    bool arrow_at_end{false};
};

// A shape between two corners, `width` pixels of outline inside its edge, mixed over the layer inside the selection.
std::vector<TilePoint> draw_shape(Layer& layer, const Selection& selection, ShapeKind kind, ShapeStyle style,
                                  Position corner, Position opposite, const Color& outline, const Color& fill,
                                  double width, bool antialias);
// A line through four points, its ends and two between them, smooth through all four, as Paint.NET's Line / Curve
// bends when its handles are dragged; `width` pixels wide, with round ends or arrowheads along its ends' direction.
std::vector<TilePoint> draw_curve(Layer& layer, const Selection& selection, const std::array<Position, 4>& through,
                                  const Color& color, double width, bool antialias, const LineSettings& settings);
// The points along such a curve, `per_span` between each two of the four, the ends included.
[[nodiscard]] std::vector<Position> curve_through(const std::array<Position, 4>& through, int per_span);
// An arc of the circle round `center` of `radius`, from the angle `start` through `sweep`, in radians as atan2 gives them
// in the image's pixels, y down: positive turns clockwise as the image shows it, and a whole turn or more is the whole
// circle. `width` pixels wide, with round ends.
std::vector<TilePoint> draw_arc(Layer& layer, const Selection& selection, Position center, double radius, double start,
                                double sweep, const Color& color, double width, bool antialias);
// A straight line `width` pixels wide with round ends, or an arrowhead at either end.
std::vector<TilePoint> draw_line(Layer& layer, const Selection& selection, Position from, Position to,
                                 const Color& color, double width, bool antialias, const LineSettings& settings);

} // namespace oui_paint

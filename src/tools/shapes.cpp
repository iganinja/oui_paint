#include "tools/shapes.h"

#include "core/blend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <set>

namespace oui_paint
{

namespace
{

// How much of a pixel whose middle is `distance` from an edge is inside, with a pixel's worth of fade across the edge,
// or none of it when not antialiased.
[[nodiscard]] double inside(double distance, bool antialias)
{
    if(!antialias)
    {
        return distance <= 0.0 ? 1.0 : 0.0;
    }
    return std::clamp(0.5 - distance, 0.0, 1.0);
}

// Distances from a point to the edge of a shape, negative inside.
[[nodiscard]] double rectangle_distance(double x, double y, double center_x, double center_y, double half_width,
                                        double half_height, double radius)
{
    const auto qx{std::abs(x - center_x) - half_width + radius};
    const auto qy{std::abs(y - center_y) - half_height + radius};
    const auto outside{std::hypot(std::max(qx, 0.0), std::max(qy, 0.0))};
    return outside + std::min(std::max(qx, qy), 0.0) - radius;
}

[[nodiscard]] double ellipse_distance(double x, double y, double center_x, double center_y, double radius_x,
                                      double radius_y)
{
    const auto dx{(x - center_x) / radius_x};
    const auto dy{(y - center_y) / radius_y};
    const auto value{dx * dx + dy * dy - 1.0};
    const auto gradient{2.0 * std::sqrt(dx * dx / (radius_x * radius_x) + dy * dy / (radius_y * radius_y))};
    return gradient > 0.0 ? value / gradient : -std::min(radius_x, radius_y);
}

[[nodiscard]] double segment_distance(double x, double y, Position from, Position to)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{dx * dx + dy * dy};
    const auto along{length > 0.0 ? std::clamp(((x - from.x) * dx + (y - from.y) * dy) / length, 0.0, 1.0) : 0.0};
    return std::hypot(from.x + along * dx - x, from.y + along * dy - y);
}

// Distance to a triangle, negative inside.
[[nodiscard]] double triangle_distance(double x, double y, const std::array<Position, 3>& corners)
{
    auto nearest{1e18};
    auto sign_changes{0};
    for(std::size_t index{}; index < 3; ++ index)
    {
        const auto& a{corners[index]};
        const auto& b{corners[(index + 1) % 3]};
        nearest = std::min(nearest, segment_distance(x, y, a, b));
        const auto cross{(b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x)};
        sign_changes += cross >= 0.0 ? 1 : 0;
    }
    const auto in{sign_changes == 0 || sign_changes == 3};
    return in ? -nearest : nearest;
}

[[nodiscard]] int tile_of(int coordinate)
{
    return coordinate >= 0 ? coordinate / tile_size : -((-coordinate + tile_size - 1) / tile_size);
}

// Mixes `color` over the layer at each pixel of the box from `left`, `top` to `right`, `bottom` by `cover(x, y)`,
// inside the selection.
template <typename Cover>
void paint(Layer& layer, const Selection& selection, double left, double top, double right, double bottom,
           const Color& color, bool antialias, Cover cover, std::set<TilePoint>& touched)
{
    const auto first_x{std::max(0, static_cast<int>(std::floor(left)) - 1)};
    const auto first_y{std::max(0, static_cast<int>(std::floor(top)) - 1)};
    const auto last_x{std::min(selection.width() - 1, static_cast<int>(std::ceil(right)) + 1)};
    const auto last_y{std::min(selection.height() - 1, static_cast<int>(std::ceil(bottom)) + 1)};
    visit_format(layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     using Channel = typename PixelType::ChannelType;
                     const auto channel{[](double value)
                                        {
                                            return static_cast<Channel>(
                                                std::lround(std::clamp(value, 0.0, 1.0) * PixelType::full));
                                        }};
                     PixelType ink;
                     if constexpr(PixelType::channels == 2)
                     {
                         ink.value = {channel(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue),
                                      channel(color.alpha)};
                     }
                     else
                     {
                         ink.value = {channel(color.red), channel(color.green), channel(color.blue),
                                      channel(color.alpha)};
                     }
                     // An opaque ink fully over a pixel replaces it, without mixing.
                     const auto opaque{ink.value.back() == PixelType::full};
                     const auto& mask{selection.mask()};
                     // A tile at a time: its pixels and the selection's are looked up once, and it is only copied to
                     // be written when something lands on it.
                     for(auto tile_y{tile_of(first_y)}; tile_y <= tile_of(last_y); ++ tile_y)
                     {
                         for(auto tile_x{tile_of(first_x)}; tile_x <= tile_of(last_x); ++ tile_x)
                         {
                             const TilePoint point{tile_x, tile_y};
                             const auto selected{mask ? mask->tile(point) : nullptr};
                             if(mask && selected == nullptr)
                             {
                                 continue;
                             }
                             Tile* tile{};
                             const auto from_y{std::max(first_y, tile_y * tile_size)};
                             const auto to_y{std::min(last_y, tile_y * tile_size + tile_size - 1)};
                             const auto from_x{std::max(first_x, tile_x * tile_size)};
                             const auto to_x{std::min(last_x, tile_x * tile_size + tile_size - 1)};
                             for(auto y{from_y}; y <= to_y; ++ y)
                             {
                                 const auto local_y{y - tile_y * tile_size};
                                 const auto* selected_row{selected != nullptr ? selected->row(local_y) : nullptr};
                                 for(auto x{from_x}; x <= to_x; ++ x)
                                 {
                                     const auto local_x{x - tile_x * tile_size};
                                     const auto how_selected{selected_row != nullptr ? selected_row[local_x * 2] : 255};
                                     if(how_selected == 0)
                                     {
                                         continue;
                                     }
                                     auto amount{cover(x + 0.5, y + 0.5) * how_selected / 255.0};
                                     // Without antialiasing nothing is mixed, the selection's soft edge neither.
                                     if(!antialias)
                                     {
                                         amount = amount >= 0.5 ? 1.0 : 0.0;
                                     }
                                     if(amount <= 0.0)
                                     {
                                         continue;
                                     }
                                     if(tile == nullptr)
                                     {
                                         tile = &layer.writable_tile(point);
                                         touched.insert(point);
                                     }
                                     tile->set_pixel(local_x, local_y,
                                                     opaque && amount >= 1.0
                                                         ? ink
                                                         : blend(tile->template pixel<PixelType>(local_x, local_y),
                                                                 ink, amount, BlendMode::normal));
                                 }
                             }
                         }
                     }
                 });
}

} // namespace

std::string_view shape_name(ShapeKind kind)
{
    switch(kind)
    {
    case ShapeKind::rectangle:
        return "Rectangle";
    case ShapeKind::rounded_rectangle:
        return "Rounded Rectangle";
    case ShapeKind::ellipse:
        break;
    }
    return "Ellipse";
}

std::string_view shape_style_name(ShapeStyle style)
{
    switch(style)
    {
    case ShapeStyle::outline:
        return "Draw Shape Outline";
    case ShapeStyle::filled:
        return "Draw Filled Shape";
    case ShapeStyle::outline_and_fill:
        break;
    }
    return "Draw Filled Shape With Outline";
}

std::vector<TilePoint> draw_shape(Layer& layer, const Selection& selection, ShapeKind kind, ShapeStyle style,
                                  Position corner, Position opposite, const Color& outline, const Color& fill,
                                  double width, bool antialias)
{
    const auto left{std::min(corner.x, opposite.x)};
    const auto top{std::min(corner.y, opposite.y)};
    const auto right{std::max(corner.x, opposite.x)};
    const auto bottom{std::max(corner.y, opposite.y)};
    const auto center_x{(left + right) / 2.0};
    const auto center_y{(top + bottom) / 2.0};
    const auto half_width{(right - left) / 2.0};
    const auto half_height{(bottom - top) / 2.0};
    const auto radius{kind == ShapeKind::rounded_rectangle ? std::min({half_width, half_height, 20.0}) : 0.0};
    const auto distance{[&](double x, double y)
                        {
                            if(kind == ShapeKind::ellipse)
                            {
                                return ellipse_distance(x, y, center_x, center_y, std::max(0.5, half_width),
                                                        std::max(0.5, half_height));
                            }
                            return rectangle_distance(x, y, center_x, center_y, half_width, half_height, radius);
                        }};
    std::set<TilePoint> touched;
    if(style != ShapeStyle::outline)
    {
        const auto& inner{style == ShapeStyle::filled ? outline : fill};
        // Filled under the outline, as far as the outline's middle, so the two meet without a gap.
        const auto inset{style == ShapeStyle::outline_and_fill ? width / 2.0 : 0.0};
        paint(layer, selection, left, top, right, bottom, inner, antialias,
              [&](double x, double y) { return inside(distance(x, y) + inset, antialias); }, touched);
    }
    if(style != ShapeStyle::filled)
    {
        // The outline inside the edge, `width` deep.
        paint(layer, selection, left, top, right, bottom, outline, antialias,
              [&](double x, double y)
              {
                  const auto from_edge{distance(x, y)};
                  return std::min(inside(from_edge, antialias), inside(-from_edge - width, antialias));
              },
              touched);
    }
    return {touched.begin(), touched.end()};
}

std::vector<TilePoint> draw_line(Layer& layer, const Selection& selection, Position from, Position to,
                                 const Color& color, double width, bool antialias, const LineSettings& settings)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{std::hypot(dx, dy)};
    // Arrowheads three widths long and wide, at least eight pixels.
    const auto head{std::max(8.0, width * 3.0)};
    const auto ux{length > 0.0 ? dx / length : 1.0};
    const auto uy{length > 0.0 ? dy / length : 0.0};
    const auto arrow{[&](Position tip, double direction) -> std::array<Position, 3>
                     {
                         const Position back{tip.x - ux * head * direction, tip.y - uy * head * direction};
                         return {tip, Position{back.x - uy * head / 2.0, back.y + ux * head / 2.0},
                                 Position{back.x + uy * head / 2.0, back.y - ux * head / 2.0}};
                     }};
    // The line stops where an arrowhead begins.
    auto start{from};
    auto end{to};
    if(settings.arrow_at_start)
    {
        start = {from.x + ux * head * 0.8, from.y + uy * head * 0.8};
    }
    if(settings.arrow_at_end)
    {
        end = {to.x - ux * head * 0.8, to.y - uy * head * 0.8};
    }
    const auto start_head{arrow(from, -1.0)};
    const auto end_head{arrow(to, 1.0)};
    const auto reach{std::max(width, head)};
    std::set<TilePoint> touched;
    paint(layer, selection, std::min(from.x, to.x) - reach, std::min(from.y, to.y) - reach,
          std::max(from.x, to.x) + reach, std::max(from.y, to.y) + reach, color, antialias,
          [&](double x, double y)
          {
              auto distance{segment_distance(x, y, start, end) - width / 2.0};
              if(settings.arrow_at_start)
              {
                  distance = std::min(distance, triangle_distance(x, y, start_head));
              }
              if(settings.arrow_at_end)
              {
                  distance = std::min(distance, triangle_distance(x, y, end_head));
              }
              return inside(distance, antialias);
          },
          touched);
    return {touched.begin(), touched.end()};
}

} // namespace oui_paint

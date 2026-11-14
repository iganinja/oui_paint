#include "tools/shapes.h"

#include "core/blend.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <set>
#include <utility>

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

// Distance to a polygon, closed, negative inside by the even-odd rule.
[[nodiscard]] double polygon_distance(double x, double y, const std::vector<Position>& corners)
{
    auto nearest{1e18};
    auto in{false};
    for(std::size_t index{}, previous{corners.size() - 1}; index < corners.size(); previous = index ++)
    {
        const auto& a{corners[index]};
        const auto& b{corners[previous]};
        nearest = std::min(nearest, segment_distance(x, y, a, b));
        if((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x)
        {
            in = !in;
        }
    }
    return in ? -nearest : nearest;
}

[[nodiscard]] double circle_distance(double x, double y, Position center, double radius)
{
    return std::hypot(x - center.x, y - center.y) - radius;
}

// The corners of a star of five points in the box, its top point up, the inner ones 0.38 of the way out.
[[nodiscard]] std::vector<Position> star_corners(double center_x, double center_y, double half_width,
                                                 double half_height)
{
    constexpr auto pi{3.14159265358979323846};
    std::vector<Position> corners;
    // The star's lowest points fall short of the bottom of a circle: stretched so it fills the box from top to bottom.
    const auto reach_down{std::cos(pi / 5.0)};
    const auto radius_y{2.0 * half_height / (1.0 + reach_down)};
    const auto middle_y{center_y - half_height + radius_y};
    for(int index{}; index < 10; ++ index)
    {
        const auto angle{-pi / 2.0 + index * pi / 5.0};
        const auto out{index % 2 == 0 ? 1.0 : 0.38};
        corners.push_back({center_x + std::cos(angle) * half_width * out, middle_y + std::sin(angle) * radius_y * out});
    }
    return corners;
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
        return "Ellipse";
    case ShapeKind::speech_bubble:
        return "Speech Bubble";
    case ShapeKind::thought_bubble:
        return "Thought Bubble";
    case ShapeKind::arrow:
        return "Arrow";
    case ShapeKind::star:
        break;
    }
    return "Star";
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
    // Which way it was drawn: a bubble's tail goes to the bottom corner on that side, an arrow points that way.
    const auto towards_right{opposite.x >= corner.x};
    const auto towards_bottom{opposite.y >= corner.y};
    // A bubble's balloon takes the top four fifths of the box, its tail the rest, down to the corner it was drawn to.
    const auto balloon_x{std::max(0.5, half_width)};
    const auto balloon_y{std::max(0.5, half_height * 0.8)};
    const auto balloon_center_y{top + balloon_y};
    const auto side{towards_right ? 1.0 : -1.0};
    const std::array<Position, 3> tail{
        Position{center_x + side * balloon_x * 0.05, balloon_center_y + balloon_y * 0.5},
        Position{center_x + side * balloon_x * 0.45, balloon_center_y + balloon_y * 0.5},
        Position{center_x + side * half_width * 0.85, bottom}};
    // A cloud: an ellipse with bumps round it, as circles on its edge.
    std::vector<std::pair<Position, double>> cloud;
    // Its middle: the ellipse the bumps sit on, so each bump is half over it.
    auto bump{0.0};
    if(kind == ShapeKind::thought_bubble)
    {
        constexpr auto pi{3.14159265358979323846};
        bump = std::min(balloon_x, balloon_y) * 0.32;
        const auto ring_x{std::max(0.5, balloon_x - bump)};
        const auto ring_y{std::max(0.5, balloon_y - bump)};
        // The bumps evenly round the ring by the length along it, as many as overlap a little, so a long flat cloud
        // has as many along its sides as it needs.
        constexpr int samples{360};
        std::vector<double> along(samples + 1, 0.0);
        const auto point{[&](int index)
                         {
                             const auto angle{2.0 * pi * index / samples};
                             return Position{center_x + std::cos(angle) * ring_x,
                                             balloon_center_y + std::sin(angle) * ring_y};
                         }};
        for(int index{1}; index <= samples; ++ index)
        {
            const auto a{point(index - 1)};
            const auto b{point(index)};
            along[static_cast<std::size_t>(index)] =
                along[static_cast<std::size_t>(index - 1)] + std::hypot(b.x - a.x, b.y - a.y);
        }
        const auto perimeter{along.back()};
        const auto count{std::clamp(static_cast<int>(std::ceil(perimeter / (bump * 1.5))), 6, 64)};
        auto sample{0};
        for(int index{}; index < count; ++ index)
        {
            const auto wanted{perimeter * index / count};
            while(sample < samples && along[static_cast<std::size_t>(sample + 1)] < wanted)
            {
                ++ sample;
            }
            cloud.push_back({point(sample), bump});
        }
        // Two little bubbles going down to the corner.
        const auto little{std::min(balloon_x, half_height) * 0.09};
        cloud.push_back({{center_x + side * half_width * 0.55, balloon_center_y + balloon_y + little * 1.2}, little});
        cloud.push_back({{center_x + side * half_width * 0.8, bottom - little * 0.6}, little * 0.6});
    }
    // An arrow along the longer side: its shaft half as thick as the box, its head as long as the box is thick.
    std::vector<Position> arrow;
    if(kind == ShapeKind::arrow)
    {
        const auto along_x{right - left >= bottom - top};
        const auto length{along_x ? right - left : bottom - top};
        const auto thick{along_x ? bottom - top : right - left};
        const auto head{std::min(length * 0.6, thick)};
        const auto forward{along_x ? towards_right : towards_bottom};
        // In the arrow's own frame, from its tail at 0 to its point at `length`, across from -thick/2 to thick/2.
        for(const auto& [a, b] : std::vector<std::pair<double, double>>{{0.0, -thick / 4.0},
                                                                         {length - head, -thick / 4.0},
                                                                         {length - head, -thick / 2.0},
                                                                         {length, 0.0},
                                                                         {length - head, thick / 2.0},
                                                                         {length - head, thick / 4.0},
                                                                         {0.0, thick / 4.0}})
        {
            const auto at{forward ? a : length - a};
            arrow.push_back(along_x ? Position{left + at, center_y + b} : Position{center_x + b, top + at});
        }
    }
    const auto star{kind == ShapeKind::star ? star_corners(center_x, center_y, half_width, half_height)
                                            : std::vector<Position>{}};
    const auto distance{[&](double x, double y)
                        {
                            switch(kind)
                            {
                            case ShapeKind::ellipse:
                                return ellipse_distance(x, y, center_x, center_y, std::max(0.5, half_width),
                                                        std::max(0.5, half_height));
                            case ShapeKind::speech_bubble:
                                return std::min(
                                    ellipse_distance(x, y, center_x, balloon_center_y, balloon_x, balloon_y),
                                    triangle_distance(x, y, tail));
                            case ShapeKind::thought_bubble:
                            {
                                // The cloud's middle, the ring the bumps sit on, and the bumps.
                                auto nearest{ellipse_distance(x, y, center_x, balloon_center_y,
                                                              std::max(0.5, balloon_x - bump),
                                                              std::max(0.5, balloon_y - bump))};
                                for(const auto& [middle, bump_radius] : cloud)
                                {
                                    nearest = std::min(nearest, circle_distance(x, y, middle, bump_radius));
                                }
                                return nearest;
                            }
                            case ShapeKind::arrow:
                                return polygon_distance(x, y, arrow);
                            case ShapeKind::star:
                                return polygon_distance(x, y, star);
                            case ShapeKind::rectangle:
                            case ShapeKind::rounded_rectangle:
                                break;
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

std::vector<Position> curve_through(const std::array<Position, 4>& through, int per_span)
{
    // Catmull-Rom through the four, with a point mirrored past each end so the curve leaves them heading for the next.
    const std::array<Position, 6> points{
        Position{2.0 * through[0].x - through[1].x, 2.0 * through[0].y - through[1].y}, through[0], through[1],
        through[2], through[3], Position{2.0 * through[3].x - through[2].x, 2.0 * through[3].y - through[2].y}};
    std::vector<Position> along;
    for(std::size_t span{1}; span <= 3; ++ span)
    {
        const auto& p0{points[span - 1]};
        const auto& p1{points[span]};
        const auto& p2{points[span + 1]};
        const auto& p3{points[span + 2]};
        for(int step{}; step < per_span || (span == 3 && step == per_span); ++ step)
        {
            const auto t{static_cast<double>(step) / per_span};
            const auto t2{t * t};
            const auto t3{t2 * t};
            const auto blend{[&](double a, double b, double c, double d)
                             {
                                 return 0.5 * (2.0 * b + (c - a) * t + (2.0 * a - 5.0 * b + 4.0 * c - d) * t2 +
                                               (3.0 * b - a - 3.0 * c + d) * t3);
                             }};
            along.push_back({blend(p0.x, p1.x, p2.x, p3.x), blend(p0.y, p1.y, p2.y, p3.y)});
        }
    }
    return along;
}

std::vector<TilePoint> draw_arc(Layer& layer, const Selection& selection, Position center, double radius, double start,
                                double sweep, const Color& color, double width, bool antialias)
{
    constexpr auto turn{2.0 * 3.14159265358979323846};
    const auto whole{std::abs(sweep) >= turn - 1e-9};
    const auto end{start + sweep};
    const Position first{center.x + std::cos(start) * radius, center.y + std::sin(start) * radius};
    const Position last{center.x + std::cos(end) * radius, center.y + std::sin(end) * radius};
    // Whether a direction from the centre lies within the arc: how far round from its start, the way it turns.
    const auto within{[&](double angle)
                      {
                          if(whole)
                          {
                              return true;
                          }
                          const auto from_start{sweep >= 0.0 ? angle - start : start - angle};
                          const auto round{from_start - std::floor(from_start / turn) * turn};
                          return round <= std::abs(sweep);
                      }};
    const auto reach{radius + width};
    std::set<TilePoint> touched;
    paint(layer, selection, center.x - reach, center.y - reach, center.x + reach, center.y + reach, color, antialias,
          [&, near{std::max(0.0, radius - width / 2.0 - 1.0)}, far{radius + width / 2.0 + 1.0}](double x, double y)
          {
              const auto dx{x - center.x};
              const auto dy{y - center.y};
              // Off the band round the circle that the arc and its round ends lie in: nothing, without the sums below,
              // which most pixels of the box are.
              const auto squared{dx * dx + dy * dy};
              if(squared < near * near || squared > far * far)
              {
                  return 0.0;
              }
              // To the circle where the pixel's direction is within the arc; to the nearer end, rounded, where not.
              const auto distance{within(std::atan2(dy, dx))
                                      ? std::abs(std::hypot(dx, dy) - radius)
                                      : std::min(std::hypot(x - first.x, y - first.y), std::hypot(x - last.x, y - last.y))};
              return inside(distance - width / 2.0, antialias);
          },
          touched);
    return {touched.begin(), touched.end()};
}

std::vector<TilePoint> draw_curve(Layer& layer, const Selection& selection, const std::array<Position, 4>& through,
                                  const Color& color, double width, bool antialias, const LineSettings& settings)
{
    auto along{curve_through(through, 24)};
    const auto head{std::max(8.0, width * 3.0)};
    // An end's direction: from the point a head's length back along the curve, so a bend near it does not tip it.
    const auto direction{[&](bool at_end)
                         {
                             const auto& tip{at_end ? along.back() : along.front()};
                             Position back{tip};
                             for(std::size_t index{1}; index < along.size(); ++ index)
                             {
                                 const auto& point{at_end ? along[along.size() - 1 - index] : along[index]};
                                 back = point;
                                 if(std::hypot(point.x - tip.x, point.y - tip.y) >= head)
                                 {
                                     break;
                                 }
                             }
                             const auto length{std::hypot(tip.x - back.x, tip.y - back.y)};
                             return length > 0.0 ? Position{(tip.x - back.x) / length, (tip.y - back.y) / length}
                                                 : Position{at_end ? 1.0 : -1.0, 0.0};
                         }};
    const auto arrow{[&](Position tip, Position heading) -> std::array<Position, 3>
                     {
                         const Position back{tip.x - heading.x * head, tip.y - heading.y * head};
                         return {tip, Position{back.x - heading.y * head / 2.0, back.y + heading.x * head / 2.0},
                                 Position{back.x + heading.y * head / 2.0, back.y - heading.x * head / 2.0}};
                     }};
    const auto start_head{arrow(along.front(), direction(false))};
    const auto end_head{arrow(along.back(), direction(true))};
    // The line stops where an arrowhead begins.
    const auto trim{[&](bool at_end)
                    {
                        const auto tip{at_end ? along.back() : along.front()};
                        while(along.size() > 2)
                        {
                            const auto& next{at_end ? along[along.size() - 2] : along[1]};
                            if(std::hypot(next.x - tip.x, next.y - tip.y) >= head * 0.8)
                            {
                                break;
                            }
                            along.erase(at_end ? along.end() - 1 : along.begin());
                        }
                    }};
    if(settings.arrow_at_start)
    {
        trim(false);
    }
    if(settings.arrow_at_end)
    {
        trim(true);
    }
    const auto reach{std::max(width, head)};
    auto left{along.front().x};
    auto top{along.front().y};
    auto right{left};
    auto bottom{top};
    for(const auto& point : along)
    {
        left = std::min(left, point.x);
        top = std::min(top, point.y);
        right = std::max(right, point.x);
        bottom = std::max(bottom, point.y);
    }
    for(const auto& tip : {start_head, end_head})
    {
        for(const auto& corner : tip)
        {
            left = std::min(left, corner.x);
            top = std::min(top, corner.y);
            right = std::max(right, corner.x);
            bottom = std::max(bottom, corner.y);
        }
    }
    std::set<TilePoint> touched;
    paint(layer, selection, left - reach, top - reach, right + reach, bottom + reach, color, antialias,
          [&](double x, double y)
          {
              auto distance{1e18};
              for(std::size_t index{1}; index < along.size(); ++ index)
              {
                  const auto& a{along[index - 1]};
                  const auto& b{along[index]};
                  // Segments whose box, grown by the width, leaves the pixel out cannot be the nearest that counts.
                  const auto grown{width + 1.0};
                  if(x < std::min(a.x, b.x) - grown || x > std::max(a.x, b.x) + grown ||
                     y < std::min(a.y, b.y) - grown || y > std::max(a.y, b.y) + grown)
                  {
                      continue;
                  }
                  distance = std::min(distance, segment_distance(x, y, a, b));
              }
              distance -= width / 2.0;
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

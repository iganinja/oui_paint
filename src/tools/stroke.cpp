#include "tools/stroke.h"

#include "core/blend.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace oui_paint
{

namespace
{

[[nodiscard]] double distance_to_segment(Position point, Position from, Position to)
{
    const auto dx{to.x - from.x};
    const auto dy{to.y - from.y};
    const auto length{dx * dx + dy * dy};
    auto along{0.0};
    if(length > 0.0)
    {
        along = std::clamp(((point.x - from.x) * dx + (point.y - from.y) * dy) / length, 0.0, 1.0);
    }
    const auto x{from.x + along * dx - point.x};
    const auto y{from.y + along * dy - point.y};
    return std::sqrt(x * x + y * y);
}

template <typename PixelType> [[nodiscard]] PixelType pixel_of(const Color& color)
{
    using Channel = typename PixelType::ChannelType;
    const auto channel{[](double value)
                       { return static_cast<Channel>(std::lround(std::clamp(value, 0.0, 1.0) * PixelType::full)); }};
    PixelType pixel;
    if constexpr(PixelType::channels == 2)
    {
        pixel.value = {channel(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue), channel(color.alpha)};
    }
    else
    {
        pixel.value = {channel(color.red), channel(color.green), channel(color.blue), channel(color.alpha)};
    }
    return pixel;
}

[[nodiscard]] int tile_of(int coordinate)
{
    return coordinate >= 0 ? coordinate / tile_size : -((-coordinate + tile_size - 1) / tile_size);
}

} // namespace

Stroke::Stroke(Layer& layer, const Selection& selection, Color color, BrushSettings settings, StrokeMode mode)
    : m_layer{layer}, m_selection{selection}, m_original{layer}, m_width{selection.width()},
      m_height{selection.height()}, m_color{color}, m_settings{settings}, m_mode{mode}
{
}

double Stroke::coverage(double distance) const
{
    const auto radius{std::max(0.5, m_settings.width / 2.0)};
    if(!m_settings.antialias)
    {
        return distance <= radius ? 1.0 : 0.0;
    }
    // The edge spread over a pixel, and inside it the fade the hardness asks for.
    const auto edge{std::clamp(radius - distance + 0.5, 0.0, 1.0)};
    const auto solid{radius * std::clamp(m_settings.hardness, 0.0, 1.0)};
    if(distance <= solid || solid >= radius)
    {
        return edge;
    }
    const auto fade{1.0 - (distance - solid) / (radius - solid)};
    return std::min(edge, std::clamp(fade, 0.0, 1.0));
}

void Stroke::line_to(Position point)
{
    cover_segment(m_last.value_or(point), point);
    m_last = point;
}

void Stroke::cover_segment(Position from, Position to)
{
    const auto reach{m_settings.width / 2.0 + 1.0};
    const auto left{std::max(0, static_cast<int>(std::floor(std::min(from.x, to.x) - reach)))};
    const auto right{std::min(m_width - 1, static_cast<int>(std::ceil(std::max(from.x, to.x) + reach)))};
    const auto top{std::max(0, static_cast<int>(std::floor(std::min(from.y, to.y) - reach)))};
    const auto bottom{std::min(m_height - 1, static_cast<int>(std::ceil(std::max(from.y, to.y) + reach)))};
    std::set<TilePoint> touched;
    for(auto y{top}; y <= bottom; ++ y)
    {
        for(auto x{left}; x <= right; ++ x)
        {
            const auto covered{coverage(distance_to_segment({x + 0.5, y + 0.5}, from, to)) *
                               m_selection.at(x, y) / 255.0};
            if(covered <= 0.0)
            {
                continue;
            }
            const TilePoint tile{tile_of(x), tile_of(y)};
            auto& cells{m_coverage[tile]};
            if(cells.empty())
            {
                cells.assign(static_cast<std::size_t>(tile_size) * tile_size, 0.0f);
            }
            auto& cell{cells[static_cast<std::size_t>((y - tile.y * tile_size) * tile_size + (x - tile.x * tile_size))]};
            if(covered > cell)
            {
                cell = static_cast<float>(covered);
                touched.insert(tile);
            }
        }
    }
    for(const auto& tile : touched)
    {
        apply(tile, {left, top, right - left + 1, bottom - top + 1});
        m_changed.push_back(tile);
    }
}

void Stroke::apply(TilePoint point, const Area& area)
{
    // The part of the tile the area covers, in the tile's pixels.
    const auto first_x{std::max(0, area.x - point.x * tile_size)};
    const auto last_x{std::min(tile_size, area.x + area.width - point.x * tile_size)};
    const auto first_y{std::max(0, area.y - point.y * tile_size)};
    const auto last_y{std::min(tile_size, area.y + area.height - point.y * tile_size)};
    const auto& cells{m_coverage[point]};
    const auto original{m_original.tile(point)};
    auto& tile{m_layer.writable_tile(point)};
    visit_format(m_layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     const auto paint{pixel_of<PixelType>(m_color)};
                     for(auto y{first_y}; y < last_y; ++ y)
                     {
                         for(auto x{first_x}; x < last_x; ++ x)
                         {
                             const auto covered{cells[static_cast<std::size_t>(y * tile_size + x)]};
                             if(covered <= 0.0f)
                             {
                                 continue;
                             }
                             const auto under{original != nullptr ? original->template pixel<PixelType>(x, y)
                                                                  : PixelType{}};
                             if(m_mode == StrokeMode::paint)
                             {
                                 tile.set_pixel(x, y, blend(under, paint, covered, BlendMode::normal));
                             }
                             else
                             {
                                 auto erased{under};
                                 auto& alpha{erased.value[PixelType::channels - 1]};
                                 alpha = static_cast<typename PixelType::ChannelType>(
                                     std::lround(alpha * (1.0 - covered * m_color.alpha)));
                                 tile.set_pixel(x, y, erased);
                             }
                         }
                     }
                 });
}

namespace
{

template <typename Change> std::vector<TilePoint> change_selected(Layer& layer, const Selection& selection, Change change)
{
    const auto area{selection.bounds()};
    std::set<TilePoint> touched;
    visit_format(layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     for(auto y{area.y}; y < area.y + area.height; ++ y)
                     {
                         for(auto x{area.x}; x < area.x + area.width; ++ x)
                         {
                             const auto amount{selection.at(x, y) / 255.0};
                             if(amount <= 0.0)
                             {
                                 continue;
                             }
                             const TilePoint point{x / tile_size, y / tile_size};
                             auto& tile{layer.writable_tile(point)};
                             const auto local_x{x % tile_size};
                             const auto local_y{y % tile_size};
                             tile.set_pixel(local_x, local_y,
                                            change(tile.template pixel<PixelType>(local_x, local_y), amount));
                             touched.insert(point);
                         }
                     }
                 });
    return {touched.begin(), touched.end()};
}

} // namespace

std::vector<TilePoint> fill_selected(Layer& layer, const Selection& selection, const Color& color)
{
    return change_selected(layer, selection,
                           [&color](auto pixel, double amount)
                           {
                               using PixelType = decltype(pixel);
                               return blend(pixel, pixel_of<PixelType>(color), amount, BlendMode::normal);
                           });
}

std::vector<TilePoint> erase_selected(Layer& layer, const Selection& selection)
{
    return change_selected(layer, selection,
                           [](auto pixel, double amount)
                           {
                               using PixelType = decltype(pixel);
                               auto& alpha{pixel.value[PixelType::channels - 1]};
                               alpha = static_cast<typename PixelType::ChannelType>(std::lround(alpha * (1.0 - amount)));
                               return pixel;
                           });
}

Lifted lift_selected(const Layer& layer, const Selection& selection)
{
    const auto area{selection.bounds()};
    Lifted lifted{layer.read(area.x, area.y, area.width, area.height), area.x, area.y};
    visit_format(layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     for(int y{}; y < area.height; ++ y)
                     {
                         for(int x{}; x < area.width; ++ x)
                         {
                             auto pixel{lifted.pixels.pixel<PixelType>(x, y)};
                             auto& alpha{pixel.value[PixelType::channels - 1]};
                             alpha = static_cast<typename PixelType::ChannelType>(
                                 (alpha * selection.at(area.x + x, area.y + y) + 127u) / 255u);
                             lifted.pixels.set_pixel(x, y, pixel);
                         }
                     }
                 });
    return lifted;
}

std::vector<TilePoint> lay_over(Layer& layer, const Image& image, int x, int y)
{
    std::set<TilePoint> touched;
    visit_format(layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     for(int row{}; row < image.height; ++ row)
                     {
                         for(int column{}; column < image.width; ++ column)
                         {
                             const auto above{image.pixel<PixelType>(column, row)};
                             const auto at_x{x + column};
                             const auto at_y{y + row};
                             if(above.alpha() == 0 || at_x < 0 || at_y < 0)
                             {
                                 continue;
                             }
                             const TilePoint point{at_x / tile_size, at_y / tile_size};
                             auto& tile{layer.writable_tile(point)};
                             const auto below{tile.template pixel<PixelType>(at_x % tile_size, at_y % tile_size)};
                             tile.set_pixel(at_x % tile_size, at_y % tile_size,
                                            blend(below, above, 1.0, BlendMode::normal));
                             touched.insert(point);
                         }
                     }
                 });
    return {touched.begin(), touched.end()};
}

std::vector<TilePoint> Stroke::take_changed()
{
    std::sort(m_changed.begin(), m_changed.end());
    m_changed.erase(std::unique(m_changed.begin(), m_changed.end()), m_changed.end());
    return std::exchange(m_changed, {});
}

} // namespace oui_paint

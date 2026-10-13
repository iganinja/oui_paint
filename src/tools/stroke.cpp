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

Stroke::Stroke(Layer& layer, int width, int height, Color color, BrushSettings settings, StrokeMode mode)
    : m_layer{layer}, m_original{layer}, m_width{width}, m_height{height}, m_color{color}, m_settings{settings},
      m_mode{mode}
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
            const auto covered{coverage(distance_to_segment({x + 0.5, y + 0.5}, from, to))};
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

std::vector<TilePoint> Stroke::take_changed()
{
    std::sort(m_changed.begin(), m_changed.end());
    m_changed.erase(std::unique(m_changed.begin(), m_changed.end()), m_changed.end());
    return std::exchange(m_changed, {});
}

} // namespace oui_paint

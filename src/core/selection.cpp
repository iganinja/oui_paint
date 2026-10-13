#include "core/selection.h"

#include <algorithm>
#include <cmath>

namespace oui_paint
{

namespace
{

[[nodiscard]] std::uint8_t combined(std::uint8_t had, std::uint8_t shape, SelectionMode mode)
{
    switch(mode)
    {
    case SelectionMode::replace:
        return shape;
    case SelectionMode::add:
        return std::max(had, shape);
    case SelectionMode::subtract:
        return static_cast<std::uint8_t>((had * (255 - shape) + 127) / 255);
    case SelectionMode::intersect:
        return std::min(had, shape);
    case SelectionMode::invert:
        break;
    }
    return static_cast<std::uint8_t>(std::abs(had - shape));
}

} // namespace

Selection::Selection(int width, int height) : m_width{width}, m_height{height}
{
}

std::uint8_t Selection::at(int x, int y) const
{
    if(!m_mask)
    {
        return 255;
    }
    if(x < 0 || y < 0 || x >= m_width || y >= m_height)
    {
        return 0;
    }
    const auto tile{m_mask->tile({x / tile_size, y / tile_size})};
    return tile == nullptr ? 0 : tile->pixel<GrayAlpha8>(x % tile_size, y % tile_size).value[0];
}

void Selection::set(int x, int y, std::uint8_t value)
{
    m_mask->writable_tile({x / tile_size, y / tile_size})
        .set_pixel(x % tile_size, y % tile_size, GrayAlpha8{{value, 255}});
}

PixelArea Selection::bounds() const
{
    if(!m_mask)
    {
        return {0, 0, m_width, m_height};
    }
    auto left{m_width};
    auto top{m_height};
    auto right{-1};
    auto bottom{-1};
    for(const auto& [point, tile] : m_mask->tiles())
    {
        for(int y{}; y < tile_size; ++ y)
        {
            for(int x{}; x < tile_size; ++ x)
            {
                if(tile->pixel<GrayAlpha8>(x, y).value[0] == 0)
                {
                    continue;
                }
                const auto image_x{point.x * tile_size + x};
                const auto image_y{point.y * tile_size + y};
                left = std::min(left, image_x);
                top = std::min(top, image_y);
                right = std::max(right, image_x);
                bottom = std::max(bottom, image_y);
            }
        }
    }
    if(right < 0)
    {
        return {};
    }
    return {left, top, right - left + 1, bottom - top + 1};
}

void Selection::select_all()
{
    clear();
    add_rectangle({0, 0, m_width, m_height}, SelectionMode::replace);
}

void Selection::invert()
{
    if(!m_mask)
    {
        // Everything is selected, so nothing will be: an empty selection, not no selection.
        m_mask.emplace(PixelFormat::gray_alpha8, "Selection");
        return;
    }
    combine({0, 0, m_width, m_height}, SelectionMode::invert, [](int, int) { return 1.0; });
}

template <typename Cover> void Selection::combine(const PixelArea& requested, SelectionMode mode, Cover cover)
{
    const auto first{!m_mask.has_value()};
    if(first)
    {
        m_mask.emplace(PixelFormat::gray_alpha8, "Selection");
    }
    if(mode == SelectionMode::replace && !first)
    {
        m_mask.emplace(PixelFormat::gray_alpha8, "Selection");
    }
    // Outside the shape, a replaced or intersected selection loses what it had.
    if(mode == SelectionMode::intersect)
    {
        const auto kept{*m_mask};
        m_mask.emplace(PixelFormat::gray_alpha8, "Selection");
        const auto left{std::max(0, requested.x)};
        const auto top{std::max(0, requested.y)};
        const auto right{std::min(m_width, requested.x + requested.width)};
        const auto bottom{std::min(m_height, requested.y + requested.height)};
        for(auto y{top}; y < bottom; ++ y)
        {
            for(auto x{left}; x < right; ++ x)
            {
                const auto tile{kept.tile({x / tile_size, y / tile_size})};
                const std::uint8_t had{tile == nullptr ? std::uint8_t{0}
                                                       : tile->pixel<GrayAlpha8>(x % tile_size, y % tile_size).value[0]};
                const auto shape{static_cast<std::uint8_t>(std::lround(std::clamp(cover(x, y), 0.0, 1.0) * 255.0))};
                const auto value{std::min(had, shape)};
                if(value != 0)
                {
                    set(x, y, value);
                }
            }
        }
        return;
    }
    const auto left{std::max(0, requested.x)};
    const auto top{std::max(0, requested.y)};
    const auto right{std::min(m_width, requested.x + requested.width)};
    const auto bottom{std::min(m_height, requested.y + requested.height)};
    for(auto y{top}; y < bottom; ++ y)
    {
        for(auto x{left}; x < right; ++ x)
        {
            const auto shape{static_cast<std::uint8_t>(std::lround(std::clamp(cover(x, y), 0.0, 1.0) * 255.0))};
            const auto had{at(x, y)};
            const auto value{combined(had, shape, mode)};
            if(value != had)
            {
                set(x, y, value);
            }
        }
    }
}

void Selection::add_rectangle(const PixelArea& area, SelectionMode mode)
{
    combine(area, mode, [](int, int) { return 1.0; });
}

void Selection::add_ellipse(double left, double top, double right, double bottom, SelectionMode mode)
{
    const auto center_x{(left + right) / 2.0};
    const auto center_y{(top + bottom) / 2.0};
    const auto radius_x{std::max(0.5, (right - left) / 2.0)};
    const auto radius_y{std::max(0.5, (bottom - top) / 2.0)};
    const PixelArea area{static_cast<int>(std::floor(left)), static_cast<int>(std::floor(top)),
                         static_cast<int>(std::ceil(right)) - static_cast<int>(std::floor(left)) + 1,
                         static_cast<int>(std::ceil(bottom)) - static_cast<int>(std::floor(top)) + 1};
    combine(area, mode,
            [&](int x, int y)
            {
                // How far inside the edge the pixel's middle is, in pixels, by the ellipse's distance function over its
                // gradient: a pixel's worth of fade across the edge.
                const auto dx{(x + 0.5 - center_x) / radius_x};
                const auto dy{(y + 0.5 - center_y) / radius_y};
                const auto value{dx * dx + dy * dy - 1.0};
                const auto gradient{2.0 * std::sqrt(dx * dx / (radius_x * radius_x) + dy * dy / (radius_y * radius_y))};
                const auto distance{gradient > 0.0 ? value / gradient : value};
                return 0.5 - distance;
            });
}

void Selection::offset(int dx, int dy)
{
    if(!m_mask || (dx == 0 && dy == 0))
    {
        return;
    }
    const auto area{bounds()};
    const auto moved{m_mask->read(area.x, area.y, area.width, area.height)};
    m_mask.emplace(PixelFormat::gray_alpha8, "Selection");
    // What moves outside the image is lost, as Paint.NET's selection stays inside it.
    const auto left{std::max(0, area.x + dx)};
    const auto top{std::max(0, area.y + dy)};
    const auto right{std::min(m_width, area.x + dx + area.width)};
    const auto bottom{std::min(m_height, area.y + dy + area.height)};
    for(auto y{top}; y < bottom; ++ y)
    {
        for(auto x{left}; x < right; ++ x)
        {
            const auto value{moved.pixel<GrayAlpha8>(x - area.x - dx, y - area.y - dy).value[0]};
            if(value != 0)
            {
                set(x, y, value);
            }
        }
    }
}

} // namespace oui_paint

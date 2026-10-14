#include "ui/anchor_grid.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

AnchorGrid::AnchorGrid(Icons& icons, int anchor) : m_icons{icons}, m_anchor{std::clamp(anchor, 0, 8)}
{
}

void AnchorGrid::set_anchor(int anchor)
{
    m_anchor = std::clamp(anchor, 0, 8);
}

void AnchorGrid::choose(int anchor)
{
    anchor = std::clamp(anchor, 0, 8);
    if(anchor == m_anchor)
    {
        return;
    }
    m_anchor = anchor;
    if(m_changed)
    {
        m_changed(m_anchor);
    }
}

oui::Size AnchorGrid::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({3 * square_width, 3 * square_height});
}

void AnchorGrid::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    const auto anchor_x{m_anchor % 3};
    const auto anchor_y{m_anchor / 3};
    for(int row{}; row < 3; ++ row)
    {
        for(int column{}; column < 3; ++ column)
        {
            const oui::Rect cells{m_bounds.x + column * square_width, m_bounds.y + row * square_height, square_width,
                                  square_height};
            const auto area{painter.pixel_bounds(cells)};
            if(!area || area->width < 6 || area->height < 6)
            {
                continue;
            }
            const auto chosen{column == anchor_x && row == anchor_y};
            // A light square with a darker edge, the chosen one tinted as what is selected.
            m_drawing.resize(area->width, area->height, 0xE8E6E0);
            const auto edge{0x8A8478u};
            m_drawing.fill(0, 0, area->width, 1, edge);
            m_drawing.fill(0, area->height - 1, area->width, 1, edge);
            m_drawing.fill(0, 0, 1, area->height, edge);
            m_drawing.fill(area->width - 1, 0, 1, area->height, edge);
            if(chosen)
            {
                m_drawing.fill(1, 1, area->width - 2, area->height - 2, 0xC8DCF0);
            }
            const auto dx{column - anchor_x};
            const auto dy{row - anchor_y};
            if(!chosen && std::abs(dx) <= 1 && std::abs(dy) <= 1)
            {
                // An arrow away from the anchor, where the canvas grows.
                const auto cx{area->width / 2};
                const auto cy{area->height / 2};
                const auto reach{std::min(area->width, area->height) / 3};
                const auto length{std::hypot(dx, dy)};
                const auto ux{dx / length};
                const auto uy{dy / length};
                const auto tip_x{cx + static_cast<int>(std::lround(ux * reach))};
                const auto tip_y{cy + static_cast<int>(std::lround(uy * reach))};
                const auto tail_x{cx - static_cast<int>(std::lround(ux * reach))};
                const auto tail_y{cy - static_cast<int>(std::lround(uy * reach))};
                constexpr std::uint32_t ink{0x202020};
                for(int thick{-1}; thick <= 0; ++ thick)
                {
                    m_drawing.line(tail_x + (dy != 0 ? thick : 0), tail_y + (dx != 0 ? thick : 0),
                                   tip_x + (dy != 0 ? thick : 0), tip_y + (dx != 0 ? thick : 0), ink);
                }
                // The head: two strokes back from the tip, either side of the shaft.
                const auto head{reach * 0.8};
                for(const auto side : {-1.0, 1.0})
                {
                    const auto back_x{ux * -head + uy * side * head * 0.7};
                    const auto back_y{uy * -head - ux * side * head * 0.7};
                    m_drawing.line(tip_x, tip_y, tip_x + static_cast<int>(std::lround(back_x)),
                                   tip_y + static_cast<int>(std::lround(back_y)), ink);
                    m_drawing.line(tip_x, tip_y - 1, tip_x + static_cast<int>(std::lround(back_x)),
                                   tip_y - 1 + static_cast<int>(std::lround(back_y)), ink);
                }
            }
            if(chosen)
            {
                // The image, where it stays, in the middle of its square.
                const auto side{std::min(area->width, area->height) * 4 / 5};
                if(const auto* const picture{m_icons.picture("Image", side, side)})
                {
                    const auto left{(area->width - picture->width) / 2};
                    const auto top{(area->height - picture->height) / 2};
                    for(int y{}; y < picture->height; ++ y)
                    {
                        for(int x{}; x < picture->width; ++ x)
                        {
                            const auto over{picture->pixels[static_cast<std::size_t>(y) * picture->width + x]};
                            const auto alpha{over >> 24u};
                            auto& under{
                                m_drawing.pixels[static_cast<std::size_t>(top + y) * m_drawing.width + left + x]};
                            const auto mix{[alpha](std::uint32_t a, std::uint32_t b)
                                           { return (a * alpha + b * (255u - alpha) + 127u) / 255u; }};
                            under = mix(over >> 16u & 0xffu, under >> 16u & 0xffu) << 16u |
                                    mix(over >> 8u & 0xffu, under >> 8u & 0xffu) << 8u |
                                    mix(over & 0xffu, under & 0xffu);
                        }
                    }
                }
            }
            painter.draw_image(cells, {m_drawing.pixels.data(), m_drawing.width, m_drawing.height, m_drawing.width},
                               theme);
        }
    }
    if(is_focused())
    {
        painter.draw_field_frame({m_bounds.x, m_bounds.y, 3 * square_width, 3 * square_height}, true, theme);
    }
}

bool AnchorGrid::handle_event(const oui::Event& event)
{
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)}; key != nullptr && !key->control && !key->alt)
    {
        const auto x{m_anchor % 3};
        const auto y{m_anchor / 3};
        switch(key->key)
        {
        case oui::Key::left:
            choose(y * 3 + std::max(0, x - 1));
            return true;
        case oui::Key::right:
            choose(y * 3 + std::min(2, x + 1));
            return true;
        case oui::Key::up:
            choose(std::max(0, y - 1) * 3 + x);
            return true;
        case oui::Key::down:
            choose(std::min(2, y + 1) * 3 + x);
            return true;
        default:
            return false;
        }
    }
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed || mouse->right)
    {
        return false;
    }
    const auto column{(mouse->x - m_bounds.x) / square_width};
    const auto row{(mouse->y - m_bounds.y) / square_height};
    if(column < 0 || column > 2 || row < 0 || row > 2)
    {
        return false;
    }
    choose(row * 3 + column);
    return true;
}

} // namespace oui_paint

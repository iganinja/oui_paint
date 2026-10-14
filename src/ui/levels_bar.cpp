#include "ui/levels_bar.h"

#include <oui/painter.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

namespace
{

// Room either side for the markers at the ends, and how tall a marker is.
constexpr int margin{10};
constexpr int marker_size{10};

} // namespace

LevelsBar::LevelsBar(int rows) : m_rows{rows}
{
}

oui::Size LevelsBar::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({36, m_rows});
}

std::optional<double> LevelsBar::place_of(int pixel_x) const
{
    if(!m_pixels || m_pixels->width <= 2 * margin + 1)
    {
        return std::nullopt;
    }
    return std::clamp(static_cast<double>(pixel_x - m_pixels->x - margin) / (m_pixels->width - 2 * margin - 1), 0.0,
                      1.0);
}

void LevelsBar::paint(oui::Painter& painter, const oui::Theme&) const
{
    m_pixels = painter.pixel_bounds(m_bounds);
    if(!m_pixels || m_pixels->width <= 2 * margin + 1 || m_pixels->height <= marker_size + 2)
    {
        return;
    }
    constexpr std::uint32_t background{0x202020};
    m_drawing.resize(m_pixels->width, m_pixels->height, background);
    const auto side{m_pixels->width - 2 * margin - 1};
    const auto bar_height{m_pixels->height - marker_size - 1};
    for(int column{}; column <= side; ++ column)
    {
        const auto level{std::min(255, column * 256 / (side + 1))};
        if(m_histogram)
        {
            const auto tallest{*std::max_element(m_histogram->begin(), m_histogram->end())};
            const auto count{(*m_histogram)[static_cast<std::size_t>(level)]};
            const auto height{tallest > 0.0 ? static_cast<int>(std::lround(count / tallest * bar_height)) : 0};
            m_drawing.fill(margin + column, bar_height - height, 1, height, 0xB8B4A8);
        }
        else
        {
            const auto gray{static_cast<std::uint32_t>(level)};
            m_drawing.fill(margin + column, 0, 1, bar_height, gray << 16 | gray << 8 | gray);
        }
    }
    for(const auto& [place, color] : m_markers)
    {
        m_drawing.marker(margin + static_cast<int>(std::lround(place * side)), bar_height + 1, marker_size, color,
                         0xE8E8E0);
    }
    painter.draw_image(m_bounds, {m_drawing.pixels.data(), m_drawing.width, m_drawing.height, m_drawing.width}, {});
}

bool LevelsBar::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pixel || mouse->right)
    {
        return false;
    }
    const auto place{place_of(mouse->pixel->x)};
    if(!place)
    {
        return false;
    }
    if(mouse->pressed)
    {
        // The nearest marker, the last of those in the same place so a dragged end can come back.
        std::optional<std::size_t> nearest;
        auto best{1e9};
        for(std::size_t index{}; index < m_markers.size(); ++ index)
        {
            const auto distance{std::abs(m_markers[index].first - *place)};
            if(distance <= best)
            {
                best = distance;
                nearest = index;
            }
        }
        m_held = nearest;
    }
    else if(!mouse->dragging && !mouse->released)
    {
        return false;
    }
    if(m_held && m_moved && !mouse->released)
    {
        m_moved(*m_held, *place);
    }
    if(mouse->released)
    {
        m_held.reset();
    }
    return true;
}

} // namespace oui_paint

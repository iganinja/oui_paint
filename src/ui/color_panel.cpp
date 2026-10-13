#include "ui/color_panel.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace olagarro;

namespace oui_paint
{

namespace
{

[[nodiscard]] oui::Color on_screen(const Color& color)
{
    // Seen through, as over the panel's grey.
    const auto channel{[&color](double value)
                       {
                           return static_cast<std::uint8_t>(
                               std::lround((value * color.alpha + 0.8 * (1.0 - color.alpha)) * 255.0));
                       }};
    return {channel(color.red), channel(color.green), channel(color.blue)};
}

[[nodiscard]] Color gray(double value)
{
    return {value, value, value, 1.0};
}

} // namespace

ColorPanel::ColorPanel()
{
    // Greys from black to white first, as heightmaps need them, then the colours.
    for(int step{}; step <= 8; ++ step)
    {
        m_palette.push_back(gray(step / 8.0));
    }
    m_palette.push_back({0.0, 0.0, 0.0, 0.0});
    for(const auto& color : std::vector<Color>{{1, 0, 0, 1},     {1, 0.5, 0, 1},   {1, 1, 0, 1},  {0.5, 1, 0, 1},
                                               {0, 1, 0, 1},     {0, 1, 0.5, 1},   {0, 1, 1, 1},  {0, 0.5, 1, 1},
                                               {0, 0, 1, 1},     {0.5, 0, 1, 1},   {1, 0, 1, 1},  {1, 0, 0.5, 1},
                                               {0.5, 0, 0, 1},   {0.5, 0.25, 0, 1}, {0, 0.5, 0, 1}, {0, 0, 0.5, 1}})
    {
        m_palette.push_back(color);
    }
}

void ColorPanel::swap()
{
    std::swap(m_primary, m_secondary);
}

void ColorPanel::on_edit(std::function<void(bool primary)> edit)
{
    m_edit = std::move(edit);
}

void ColorPanel::on_picked(std::function<void(bool secondary)> picked)
{
    m_picked = std::move(picked);
}

std::optional<Color> ColorPanel::palette_at(oui::Point cell) const
{
    if(cell.y < squares_rows || cell.x < 0 || cell.x >= width)
    {
        return std::nullopt;
    }
    // Two cells to a colour, so each is square, two to a row.
    const auto index{static_cast<std::size_t>((cell.y - squares_rows) * 2 + cell.x / 2)};
    if(index >= m_palette.size())
    {
        return std::nullopt;
    }
    return m_palette[index];
}

oui::Size ColorPanel::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({width, squares_rows + static_cast<int>((m_palette.size() + 1) / 2)});
}

void ColorPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, theme.status_background);
    // The secondary square behind, down and to the right, the primary in front.
    painter.fill_rect({m_bounds.x + 1, m_bounds.y + 1, 3, 2}, on_screen(m_secondary));
    painter.fill_rect({m_bounds.x, m_bounds.y, 3, 2}, on_screen(m_primary));
    for(std::size_t index{}; index < m_palette.size(); ++ index)
    {
        const auto x{m_bounds.x + static_cast<int>(index % 2) * 2};
        const auto y{m_bounds.y + squares_rows + static_cast<int>(index / 2)};
        painter.fill_rect({x, y, 2, 1}, on_screen(m_palette[index]));
    }
}

bool ColorPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed)
    {
        return false;
    }
    const oui::Point cell{mouse->x - m_bounds.x, mouse->y - m_bounds.y};
    if(cell.y < squares_rows)
    {
        if(m_edit)
        {
            // The primary square is the top left one; what shows of the secondary, below and to its right.
            m_edit(cell.y < 2 && cell.x < 3);
        }
        return true;
    }
    if(const auto color{palette_at(cell)})
    {
        if(mouse->right)
        {
            m_secondary = *color;
        }
        else
        {
            m_primary = *color;
        }
        if(m_picked)
        {
            m_picked(mouse->right);
        }
        return true;
    }
    return false;
}

} // namespace oui_paint

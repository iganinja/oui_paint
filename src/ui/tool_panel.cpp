#include "ui/tool_panel.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <utility>

using namespace olagarro;

namespace oui_paint
{

ToolPanel::ToolPanel(std::vector<ToolButton> buttons, Icons& icons) : m_buttons{std::move(buttons)}, m_icons{icons}
{
    if(!m_buttons.empty())
    {
        m_selected = m_buttons.front().id;
    }
}

void ToolPanel::set_selected(std::string_view id)
{
    m_selected = std::string{id};
}

const std::string& ToolPanel::selected() const
{
    return m_selected;
}

void ToolPanel::on_picked(std::function<void(const std::string&)> picked)
{
    m_picked = std::move(picked);
}

std::optional<std::size_t> ToolPanel::button_at(oui::Point cell) const
{
    if(cell.x < 0 || cell.y < 0 || cell.x >= columns * button_width)
    {
        return std::nullopt;
    }
    const auto index{static_cast<std::size_t>(cell.y * columns + cell.x / button_width)};
    if(index >= m_buttons.size())
    {
        return std::nullopt;
    }
    return index;
}

oui::Rect ToolPanel::button_bounds(std::size_t index) const
{
    const auto column{static_cast<int>(index) % columns};
    const auto row{static_cast<int>(index) / columns};
    return {m_bounds.x + column * button_width, m_bounds.y + row, button_width, 1};
}

oui::Size ToolPanel::measure(const oui::Constraints& constraints) const
{
    const auto rows{(static_cast<int>(m_buttons.size()) + columns - 1) / columns};
    return constraints.constrain({columns * button_width, rows});
}

void ToolPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, theme.status_background);
    for(std::size_t index{}; index < m_buttons.size(); ++ index)
    {
        const auto bounds{button_bounds(index)};
        if(m_buttons[index].id == m_selected)
        {
            painter.fill_rect(bounds, theme.menu_selected_background);
        }
        const auto area{painter.pixel_bounds(bounds)};
        if(!area)
        {
            continue;
        }
        if(const auto* const picture{m_icons.picture(m_buttons[index].icon, area->width, area->height)})
        {
            oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
            image.alpha = true;
            painter.draw_image(bounds, image, theme);
        }
    }
}

bool ToolPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed || mouse->right)
    {
        return false;
    }
    const auto index{button_at({mouse->x - m_bounds.x, mouse->y - m_bounds.y})};
    if(!index)
    {
        return false;
    }
    m_selected = m_buttons[*index].id;
    if(m_picked)
    {
        m_picked(m_selected);
    }
    return true;
}

std::optional<std::string> ToolPanel::tooltip_at(oui::Point point) const
{
    if(const auto index{button_at({point.x - m_bounds.x, point.y - m_bounds.y})})
    {
        return m_buttons[*index].tooltip;
    }
    return std::nullopt;
}

} // namespace oui_paint

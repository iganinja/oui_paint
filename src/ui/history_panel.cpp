#include "ui/history_panel.h"

#include "ui/layers_panel.h"
#include "ui/side_panel.h"

#include <oui/translation.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>

using namespace olagarro;

namespace oui_paint
{

HistoryPanel::HistoryPanel(Icons& icons) : m_icons{icons}
{
}

void HistoryPanel::set_history(const History* history)
{
    m_history = history;
    m_first = 0;
    m_followed.reset();
}

void HistoryPanel::on_choose(std::function<void(std::size_t)> choose)
{
    m_choose = std::move(choose);
}

int HistoryPanel::step_rows() const
{
    return std::max(0, m_bounds.height - 1);
}

void HistoryPanel::follow() const
{
    if(m_history == nullptr || m_followed == m_history->position())
    {
        return;
    }
    const auto position{m_history->position()};
    const auto rows{static_cast<std::size_t>(std::max(1, step_rows()))};
    if(position < m_first)
    {
        m_first = position;
    }
    else if(position >= m_first + rows)
    {
        m_first = position - rows + 1;
    }
    m_followed = position;
}

std::optional<std::size_t> HistoryPanel::position_at_row(int row) const
{
    follow();
    if(m_history == nullptr || row < 0 || row >= step_rows())
    {
        return std::nullopt;
    }
    const auto position{m_first + static_cast<std::size_t>(row)};
    if(position > m_history->steps().size())
    {
        return std::nullopt;
    }
    return position;
}

oui::Size HistoryPanel::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({LayersPanel::width, constraints.max_height});
}

void HistoryPanel::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    // A new height may leave the position it is at out of sight.
    m_followed.reset();
}

void HistoryPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, side_panel_color(theme));
    for(int row{}; row < step_rows(); ++ row)
    {
        const auto position{position_at_row(row)};
        if(!position)
        {
            break;
        }
        const oui::Rect line{m_bounds.x, m_bounds.y + row, m_bounds.width, 1};
        const auto current{*position == m_history->position()};
        // What can be redone, past where it is, greyed.
        const auto undone{*position > m_history->position()};
        if(current)
        {
            painter.fill_rect(line, theme.menu_selected_background);
        }
        const oui::TextStyle style{current  ? theme.menu_selected_foreground
                                   : undone ? theme.muted_text
                                            : theme.status_foreground,
                                   current ? theme.menu_selected_background : side_panel_color(theme)};
        const auto name{*position == 0 ? std::string{oui::translate("Panels", "Original")} : m_history->steps()[*position - 1].name};
        painter.draw_label({line.x + 1, line.y}, name, style);
    }
    // Undo and redo under the steps, on a bar of their own; none without a document to undo in.
    if(m_history == nullptr || m_bounds.height < 2)
    {
        return;
    }
    const auto bottom{m_bounds.y + m_bounds.height - 1};
    painter.draw_divider({m_bounds.x, bottom, m_bounds.width, 1}, theme);
    for(const auto& [column, icon] : {std::pair<int, const char*>{0, "RotateLeft"}, {2, "RotateRight"}})
    {
        const oui::Rect cell{m_bounds.x + column, bottom, 2, 1};
        if(const auto area{painter.pixel_bounds(cell)})
        {
            if(const auto* const picture{m_icons.picture(icon, area->width, area->height)})
            {
                oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
                image.alpha = true;
                painter.draw_image(cell, image, theme);
            }
        }
    }
}

bool HistoryPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || m_history == nullptr)
    {
        return false;
    }
    if(mouse->wheel_delta != 0 && mouse->x >= m_bounds.x && mouse->x < m_bounds.x + m_bounds.width &&
       mouse->y >= m_bounds.y && mouse->y < m_bounds.y + m_bounds.height)
    {
        // Up the list for a turn away from the person, as text scrolls.
        const auto last{m_history->steps().size() + 1};
        const auto rows{static_cast<std::size_t>(std::max(1, step_rows()))};
        const auto most{last > rows ? last - rows : 0};
        const auto moved{static_cast<long long>(m_first) - mouse->wheel_delta};
        m_first = static_cast<std::size_t>(std::clamp<long long>(moved, 0, static_cast<long long>(most)));
        return true;
    }
    if(!mouse->pressed || mouse->right)
    {
        return false;
    }
    const oui::Point cell{mouse->x - m_bounds.x, mouse->y - m_bounds.y};
    if(cell.y == m_bounds.height - 1)
    {
        if(cell.x < 2 && m_undo)
        {
            m_undo();
        }
        else if(cell.x >= 2 && cell.x < 4 && m_redo)
        {
            m_redo();
        }
        return true;
    }
    if(const auto position{position_at_row(cell.y)}; position && m_choose)
    {
        m_choose(*position);
        return true;
    }
    return false;
}

std::optional<std::string> HistoryPanel::tooltip_at(oui::Point point) const
{
    if(point.y != m_bounds.y + m_bounds.height - 1)
    {
        return std::nullopt;
    }
    const auto column{point.x - m_bounds.x};
    if(column < 2)
    {
        return std::string{oui::translate("Panels", "Undo")};
    }
    if(column < 4)
    {
        return std::string{oui::translate("Panels", "Redo")};
    }
    return std::nullopt;
}

} // namespace oui_paint

#include "ui/main_frame.h"

#include <algorithm>

using namespace olagarro;

namespace oui_paint
{

MainFrame::MainFrame(oui::CommandRegistry& commands)
{
    // The status bar goes in before the menu bar, so the menus, which open over everything, are hit first.
    m_status_bar = &add(oui::status_bar());
    m_menu_bar = &add(std::make_unique<oui::MenuBar>(commands));
}

oui::MenuBar& MainFrame::menu_bar()
{
    return *m_menu_bar;
}

oui::StatusBar& MainFrame::status_bar()
{
    return *m_status_bar;
}

void MainFrame::replace_slot(oui::Widget*& slot, std::unique_ptr<oui::Widget> widget)
{
    if(slot != nullptr)
    {
        static_cast<void>(remove(*slot));
    }
    slot = widget.get();
    // First among the children, below the bars.
    static_cast<void>(add(std::move(widget)));
    auto& children{children_mutable()};
    std::rotate(children.begin(), children.end() - 1, children.end());
}

void MainFrame::set_center(std::unique_ptr<oui::Widget> center)
{
    replace_slot(m_center, std::move(center));
}

void MainFrame::set_left(std::unique_ptr<oui::Widget> left)
{
    replace_slot(m_left, std::move(left));
}

void MainFrame::set_right(std::unique_ptr<oui::Widget> right)
{
    replace_slot(m_right, std::move(right));
}

void MainFrame::set_top(std::unique_ptr<oui::Widget> top)
{
    replace_slot(m_top, std::move(top));
}

oui::Widget* MainFrame::left()
{
    return m_left;
}

oui::Widget* MainFrame::center()
{
    return m_center;
}

oui::Size MainFrame::measure(const oui::Constraints& constraints) const
{
    return {constraints.max_width, constraints.max_height};
}

void MainFrame::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    const auto menu_height{m_menu_bar->measure({bounds.width, bounds.height}).height};
    const auto status_height{m_status_bar->measure({bounds.width, bounds.height}).height};
    m_menu_bar->arrange({bounds.x, bounds.y, bounds.width, menu_height});
    m_status_bar->arrange({bounds.x, bounds.y + bounds.height - status_height, bounds.width, status_height});
    auto top_height{0};
    if(m_top != nullptr)
    {
        top_height = m_top->measure({bounds.width, bounds.height}).height;
        m_top->arrange({bounds.x, bounds.y + menu_height, bounds.width, top_height});
    }
    const auto middle_top{bounds.y + menu_height + top_height};
    const auto middle_height{std::max(0, bounds.height - menu_height - top_height - status_height)};
    auto left_width{0};
    if(m_left != nullptr)
    {
        left_width = std::min(bounds.width, m_left->measure({bounds.width, middle_height}).width);
        m_left->arrange({bounds.x, middle_top, left_width, middle_height});
    }
    auto right_width{0};
    if(m_right != nullptr)
    {
        right_width = std::min(bounds.width - left_width, m_right->measure({bounds.width, middle_height}).width);
        m_right->arrange({bounds.x + bounds.width - right_width, middle_top, right_width, middle_height});
    }
    if(m_center != nullptr)
    {
        m_center->arrange(
            {bounds.x + left_width, middle_top, bounds.width - left_width - right_width, middle_height});
    }
}

} // namespace oui_paint

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

void MainFrame::set_center(std::unique_ptr<oui::Widget> center)
{
    if(m_center != nullptr)
    {
        static_cast<void>(remove(*m_center));
    }
    m_center = center.get();
    // First among the children, below the bars.
    static_cast<void>(add(std::move(center)));
    auto& children{children_mutable()};
    std::rotate(children.begin(), children.end() - 1, children.end());
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
    if(m_center != nullptr)
    {
        const auto center_height{std::max(0, bounds.height - menu_height - status_height)};
        m_center->arrange({bounds.x, bounds.y + menu_height, bounds.width, center_height});
    }
}

} // namespace oui_paint

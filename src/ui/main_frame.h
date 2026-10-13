#pragma once

#include <oui/container.h>
#include <oui/menu_bar.h>
#include <oui/status_bar.h>

#include <memory>

namespace oui_paint
{

// The application's window contents: the menu bar along the top, the status bar along the bottom, a side panel on the
// left, as tall as the space between them and as wide as it asks, and whatever is in the middle filling the rest.
class MainFrame final : public olagarro::oui::Container
{
  public:
    explicit MainFrame(olagarro::oui::CommandRegistry& commands);

    [[nodiscard]] olagarro::oui::MenuBar& menu_bar();
    [[nodiscard]] olagarro::oui::StatusBar& status_bar();
    // What fills the middle; the one there before is dropped.
    void set_center(std::unique_ptr<olagarro::oui::Widget> center);
    [[nodiscard]] olagarro::oui::Widget* center();
    void set_left(std::unique_ptr<olagarro::oui::Widget> left);
    // A panel on the right, as tall as the space between the bars and as wide as it asks: the layers.
    void set_right(std::unique_ptr<olagarro::oui::Widget> right);
    // A bar under the menu bar, as tall as it asks, across the window: the tool's options.
    void set_top(std::unique_ptr<olagarro::oui::Widget> top);
    [[nodiscard]] olagarro::oui::Widget* left();

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    olagarro::oui::MenuBar* m_menu_bar{};
    olagarro::oui::StatusBar* m_status_bar{};
    // Puts `widget` where `slot` was, below the bars.
    void replace_slot(olagarro::oui::Widget*& slot, std::unique_ptr<olagarro::oui::Widget> widget);

    olagarro::oui::Widget* m_center{};
    olagarro::oui::Widget* m_left{};
    olagarro::oui::Widget* m_top{};
    olagarro::oui::Widget* m_right{};
};

} // namespace oui_paint

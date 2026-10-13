#pragma once

#include <oui/container.h>
#include <oui/menu_bar.h>
#include <oui/status_bar.h>

#include <memory>

namespace oui_paint
{

// The application's window contents: the menu bar along the top, the status bar along the bottom, and whatever is
// in the middle filling the rest.
class MainFrame final : public olagarro::oui::Container
{
  public:
    explicit MainFrame(olagarro::oui::CommandRegistry& commands);

    [[nodiscard]] olagarro::oui::MenuBar& menu_bar();
    [[nodiscard]] olagarro::oui::StatusBar& status_bar();
    // What fills the middle; the one there before is dropped.
    void set_center(std::unique_ptr<olagarro::oui::Widget> center);
    [[nodiscard]] olagarro::oui::Widget* center();

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    olagarro::oui::MenuBar* m_menu_bar{};
    olagarro::oui::StatusBar* m_status_bar{};
    olagarro::oui::Widget* m_center{};
};

} // namespace oui_paint

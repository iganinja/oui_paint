#pragma once

#include "ui/icons.h"

#include <oui/widget.h>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// A button of the tool panel: what it is called when picked, the icon it shows and what its tooltip says.
struct ToolButton
{
    std::string id;
    std::string icon;
    std::string tooltip;
};

// The tools as icon buttons in a grid, as Paint.NET's Tools window: one of them is the tool in use, and clicking
// another picks it.
class ToolPanel final : public olagarro::oui::Widget
{
  public:
    // Each button is this many cells wide and one row tall.
    static constexpr int button_width{2};
    static constexpr int columns{2};

    ToolPanel(std::vector<ToolButton> buttons, Icons& icons);

    void set_selected(std::string_view id);
    [[nodiscard]] const std::string& selected() const;
    // Called with the id of a button clicked, after it became the selected one.
    void on_picked(std::function<void(const std::string&)> picked);

    // The button at a cell inside the panel, counted from its top left corner; none between or past the buttons.
    [[nodiscard]] std::optional<std::size_t> button_at(olagarro::oui::Point cell) const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] std::optional<std::string> tooltip_at(olagarro::oui::Point point) const override;

  private:
    [[nodiscard]] olagarro::oui::Rect button_bounds(std::size_t index) const;

    std::vector<ToolButton> m_buttons;
    Icons& m_icons;
    std::string m_selected;
    std::function<void(const std::string&)> m_picked;
};

} // namespace oui_paint

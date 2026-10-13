#pragma once

#include "core/history.h"
#include "ui/icons.h"

#include <oui/widget.h>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

namespace oui_paint
{

// The steps of a document's history, as Paint.NET's History window lists them: the image as it was first, then each
// step by its name, the one it is at marked and those that can be redone greyed. A click on one takes the image there;
// the buttons under them undo and redo. It says what was asked; the program does it.
class HistoryPanel final : public olagarro::oui::Widget
{
  public:
    explicit HistoryPanel(Icons& icons);

    // The history it shows, none for no document.
    void set_history(const History* history);
    // Called with how many steps should be done, 0 for the image as it was first.
    void on_choose(std::function<void(std::size_t position)> choose);
    void on_undo(std::function<void()> undo) { m_undo = std::move(undo); }
    void on_redo(std::function<void()> redo) { m_redo = std::move(redo); }

    // The position a row counted from the panel's top stands for, none past the steps or on the buttons.
    [[nodiscard]] std::optional<std::size_t> position_at_row(int row) const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] std::optional<std::string> tooltip_at(olagarro::oui::Point point) const override;

  private:
    // Rows for the steps: all but the buttons'.
    [[nodiscard]] int step_rows() const;
    // Scrolls so the position it is at shows, when it changed since last looked.
    void follow() const;

    Icons& m_icons;
    const History* m_history{};
    std::function<void(std::size_t)> m_choose;
    std::function<void()> m_undo;
    std::function<void()> m_redo;
    // The first position shown, and the one it was at when last followed.
    mutable std::size_t m_first{0};
    mutable std::optional<std::size_t> m_followed;
};

} // namespace oui_paint

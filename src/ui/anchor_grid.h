#pragma once

#include "ui/icons.h"
#include "ui/pixel_drawing.h"

#include <oui/widget.h>

#include <functional>

namespace oui_paint
{

// Where an image stays as its canvas grows or shrinks, as Paint.NET's Canvas Size shows it: nine squares, the image in
// the one chosen and arrows round it pointing where the canvas grows. A click, or the arrow keys, choose another.
class AnchorGrid final : public olagarro::oui::Widget
{
  public:
    explicit AnchorGrid(Icons& icons, int anchor = 4);

    // 0 for the top left to 8 for the bottom right, row by row.
    [[nodiscard]] int anchor() const { return m_anchor; }
    void set_anchor(int anchor);
    // Another square chosen.
    void on_changed(std::function<void(int anchor)> changed) { m_changed = std::move(changed); }

    [[nodiscard]] bool is_focusable() const override { return true; }
    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    // Each square, in cells: as wide as tall, a cell being twice as tall as it is wide.
    static constexpr int square_width{4};
    static constexpr int square_height{2};
    void choose(int anchor);

    Icons& m_icons;
    int m_anchor;
    std::function<void(int)> m_changed;
    mutable PixelDrawing m_drawing;
};

} // namespace oui_paint

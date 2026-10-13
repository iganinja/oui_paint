#pragma once

#include "tools/stroke.h"

#include <oui/widget.h>

#include <functional>
#include <optional>
#include <vector>

namespace oui_paint
{

// The primary and secondary colours, as two squares one over the other, and a palette under them: a click on a
// palette colour makes it the colour being edited, the primary unless C chose the secondary, a right click the other,
// and a click on a square asks for it. The colour being edited shows in front, framed.
class ColorPanel final : public olagarro::oui::Widget
{
  public:
    static constexpr int width{4};

    ColorPanel();

    [[nodiscard]] const Color& primary() const { return m_primary; }
    [[nodiscard]] const Color& secondary() const { return m_secondary; }
    void set_primary(const Color& color) { m_primary = color; }
    void set_secondary(const Color& color) { m_secondary = color; }
    void swap();
    // Whether a click on the palette sets the secondary colour, as C toggles it.
    [[nodiscard]] bool editing_secondary() const { return m_editing_secondary; }
    void toggle_editing() { m_editing_secondary = !m_editing_secondary; }
    // Called with true for the primary square, false for the secondary, when it is clicked.
    void on_edit(std::function<void(bool primary)> edit);
    // Called after a palette colour was clicked: true for the secondary.
    void on_picked(std::function<void(bool secondary)> picked);

    // The palette colour at a cell inside the panel, counted from its top left corner.
    [[nodiscard]] std::optional<Color> palette_at(olagarro::oui::Point cell) const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    // Rows the squares take before the palette starts.
    static constexpr int squares_rows{3};

    Color m_primary{0.0, 0.0, 0.0, 1.0};
    Color m_secondary{1.0, 1.0, 1.0, 1.0};
    std::vector<Color> m_palette;
    bool m_editing_secondary{false};
    std::function<void(bool)> m_edit;
    std::function<void(bool)> m_picked;
};


} // namespace oui_paint

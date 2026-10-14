#pragma once

#include "ui/pixel_drawing.h"

#include <oui/application.h>
#include <oui/widget.h>

#include <functional>
#include <optional>

namespace oui_paint
{

// Where light comes from, as a sun on a ring round the middle: a press or a drag puts it towards the pointer, in whole
// degrees counted anticlockwise from the right, and by 15 with Shift.
class LightDial final : public olagarro::oui::Widget
{
  public:
    explicit LightDial(double degrees);

    [[nodiscard]] double degrees() const { return m_degrees; }
    // As the code sets it, which on_changed does not hear.
    void set_degrees(double degrees);
    void on_changed(std::function<void(double degrees)> changed);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    double m_degrees;
    std::function<void(double)> m_changed;
    mutable std::optional<olagarro::oui::Rect> m_pixels;
    mutable PixelDrawing m_drawing;
};

// Asks where light comes from, from `start` degrees; `changed` hears each direction as it is chosen. None when
// cancelled.
[[nodiscard]] std::optional<double> ask_light_direction(olagarro::oui::Application& application, double start,
                                                        const std::function<void(double degrees)>& changed);

} // namespace oui_paint

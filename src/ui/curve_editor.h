#pragma once

#include "effects/curve_edit.h"
#include "ui/pixel_drawing.h"

#include <oui/widget.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace oui_paint
{

// The graph of a curve, as Paint.NET's Curves dialog shows it: what goes in along the bottom, what comes out up the
// side, over the image's histogram, with the curves not being edited faint behind. CurveEdit says what the pointer
// does to its points.
class CurveEditor final : public olagarro::oui::Widget
{
  public:
    CurveEditor();

    [[nodiscard]] const std::vector<CurvePoint>& points() const { return m_edit.points(); }
    void set_points(std::vector<CurvePoint> points);
    // The colour the edited curve is drawn in, as 0xRRGGBB.
    void set_color(std::uint32_t color) { m_color = color; }
    // Curves shown behind, faint, each with its colour.
    void set_faint(std::vector<std::pair<std::vector<CurvePoint>, std::uint32_t>> faint) { m_faint = std::move(faint); }
    // How many pixels there are of each level, drawn behind the curves.
    void set_histogram(const std::array<double, 256>& histogram) { m_histogram = histogram; }
    // Its points changed by the pointer.
    void on_changed(std::function<void()> changed) { m_changed = std::move(changed); }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] bool takes_raw_pointer() const override { return true; }

  private:
    // Where a pixel of the window is on the graph, 0 to 1 both ways, up being 1; none before it was drawn.
    [[nodiscard]] std::optional<std::pair<double, double>> on_graph(olagarro::oui::Point pixel) const;

    CurveEdit m_edit;
    std::uint32_t m_color{0xE8E8E0};
    std::vector<std::pair<std::vector<CurvePoint>, std::uint32_t>> m_faint;
    std::optional<std::array<double, 256>> m_histogram;
    std::function<void()> m_changed;
    // Where it was last drawn, in the window's pixels, and its picture.
    mutable std::optional<olagarro::oui::Rect> m_pixels;
    mutable PixelDrawing m_drawing;
};

} // namespace oui_paint

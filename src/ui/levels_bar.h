#pragma once

#include "ui/pixel_drawing.h"

#include <oui/widget.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace oui_paint
{

// A bar from black to white with markers under it that the pointer drags, as Paint.NET's Levels dialog has: over an
// image's histogram for what goes in, over a gradient for what comes out. It says where a marker was dragged; whoever
// owns it decides where the markers are.
class LevelsBar final : public olagarro::oui::Widget
{
  public:
    // How tall it is, in cells: a histogram wants more room than a gradient.
    explicit LevelsBar(int rows);

    // A histogram drawn in the bar; without one, a gradient from black to white.
    void set_histogram(const std::array<double, 256>& histogram) { m_histogram = histogram; }
    // Where each marker is, from 0 to 1, and its colour as 0xRRGGBB.
    void set_markers(std::vector<std::pair<double, std::uint32_t>> markers) { m_markers = std::move(markers); }
    // A marker dragged, and where to, from 0 to 1.
    void on_moved(std::function<void(std::size_t marker, double place)> moved) { m_moved = std::move(moved); }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] bool takes_raw_pointer() const override { return true; }

  private:
    [[nodiscard]] std::optional<double> place_of(int pixel_x) const;

    int m_rows;
    std::optional<std::array<double, 256>> m_histogram;
    std::vector<std::pair<double, std::uint32_t>> m_markers;
    std::function<void(std::size_t, double)> m_moved;
    std::optional<std::size_t> m_held;
    mutable std::optional<olagarro::oui::Rect> m_pixels;
    mutable PixelDrawing m_drawing;
};

} // namespace oui_paint

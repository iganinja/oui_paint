#pragma once

#include "app/open_document.h"

#include <oui/widget.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace oui_paint
{

// What the pointer does to the image with the left or the right button, for the tool in use.
struct ToolEvent
{
    enum class Phase
    {
        press,
        move,
        release,
    };
    Phase phase{Phase::press};
    // In the image's pixels, between whole ones too: (0.5, 0.5) is the middle of the top left pixel.
    Position at;
    // The right button, which paints with the secondary colour.
    bool secondary{false};
    bool shift{false};
    bool control{false};
    bool alt{false};
};

// A document as it shows in its tab, in pixels: Ctrl and the wheel zoom around the pointer, the wheel scrolls up and
// down, Shift and the wheel sideways, and the middle button drags it about. The left and right buttons go to the
// tool in use.
class CanvasView final : public olagarro::oui::Widget
{
  public:
    explicit CanvasView(OpenDocument& document);

    [[nodiscard]] OpenDocument& document() { return m_document; }
    // The image pixel under the pointer, none when it is not over the image.
    [[nodiscard]] const std::optional<Position>& pointer() const { return m_pointer; }
    // Called after the view changed, by zoom or scrolling, or the pointer moved over it.
    void on_changed(std::function<void()> changed);
    void on_tool(std::function<void(const ToolEvent&)> tool);

    void zoom_in();
    void zoom_out();
    void zoom_to_fit();
    void actual_size();

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] bool takes_raw_pointer() const override;
    [[nodiscard]] olagarro::oui::Pointer pointer_at(olagarro::oui::Point point) const override;
    [[nodiscard]] bool is_focusable() const override;

  private:
    // The middle of the view, in its pixels.
    [[nodiscard]] Position middle() const;
    // Where a pixel of the window is in the view's pixels.
    [[nodiscard]] std::optional<Position> in_view(const olagarro::oui::MouseEvent& mouse) const;
    void changed();

    OpenDocument& m_document;
    std::function<void()> m_changed;
    std::function<void(const ToolEvent&)> m_tool;
    // The button a tool is being used with, from its press to its release.
    std::optional<bool> m_tool_button;
    std::optional<Position> m_pointer;
    std::optional<olagarro::oui::Point> m_panning_from;
    mutable bool m_fitted{false};
    // Where it was last painted, in the window's pixels, and what it painted.
    mutable std::optional<olagarro::oui::Rect> m_pixels;
    mutable std::vector<std::uint32_t> m_buffer;
};

} // namespace oui_paint

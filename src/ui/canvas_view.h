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
    // In the view's pixels, which stay put while the view scrolls, as panning needs.
    Position in_view;
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
    // Draws over the view after the image and the selection, as the floating piece's handles.
    void on_overlay(std::function<void(std::uint32_t* pixels, int width, int height)> overlay);
    // While `claiming`, the keys typed go to `keys` before the program's shortcuts, as the Text tool needs.
    void claim_keys(bool claiming) { m_claiming_keys = claiming; }
    void on_keys(std::function<bool(const olagarro::oui::Event&)> keys);
    // What the pointer looks like over the image, as the tool in use asks, told whether a button is held.
    void on_pointer(std::function<olagarro::oui::Pointer(bool held)> pointer);

    // Where the marching ants are along the selection's edge; they move as it grows.
    void set_ants_phase(int phase) { m_ants_phase = phase; }

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
    [[nodiscard]] bool claims_key(const olagarro::oui::Event& key) const override;

  private:
    // The middle of the view, in its pixels.
    [[nodiscard]] Position middle() const;
    // Where a pixel of the window is in the view's pixels.
    [[nodiscard]] std::optional<Position> in_view(const olagarro::oui::MouseEvent& mouse) const;
    void changed();

    OpenDocument& m_document;
    std::function<void()> m_changed;
    std::function<void(const ToolEvent&)> m_tool;
    std::function<olagarro::oui::Pointer(bool)> m_pointer_for_tool;
    std::function<bool(const olagarro::oui::Event&)> m_keys;
    std::function<void(std::uint32_t*, int, int)> m_overlay;
    bool m_claiming_keys{false};
    // The button a tool is being used with, from its press to its release.
    std::optional<bool> m_tool_button;
    std::optional<Position> m_pointer;
    std::optional<olagarro::oui::Point> m_panning_from;
    mutable bool m_fitted{false};
    int m_ants_phase{0};
    // Where it was last painted, in the window's pixels, and what it painted.
    mutable std::optional<olagarro::oui::Rect> m_pixels;
    mutable std::vector<std::uint32_t> m_buffer;
};

} // namespace oui_paint

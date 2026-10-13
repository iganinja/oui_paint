#pragma once

#include "tools/stroke.h"

#include <oui/container.h>

#include <functional>

namespace olagarro::oui
{
class CheckButton;
class NumberField;
class Slider;
} // namespace olagarro::oui

namespace oui_paint
{

// The bar of the brush's options, as Paint.NET's tool bar: its width, by number or slider, its hardness and whether
// its edge is antialiased.
class ToolOptions final : public olagarro::oui::Container
{
  public:
    explicit ToolOptions(BrushSettings settings);

    [[nodiscard]] const BrushSettings& settings() const { return m_settings; }
    // Width by `step` pixels, kept from 1 to the widest, as [ and ] do.
    void change_width(double step);
    // Called after any option changed.
    void on_changed(std::function<void(const BrushSettings&)> changed);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;

  private:
    void changed();

    BrushSettings m_settings;
    olagarro::oui::NumberField* m_width{};
    olagarro::oui::Slider* m_width_slider{};
    olagarro::oui::NumberField* m_hardness{};
    olagarro::oui::CheckButton* m_antialias{};
    std::function<void(const BrushSettings&)> m_changed;
    bool m_updating{false};
};

} // namespace oui_paint

#pragma once

#include "tools/fill.h"
#include "tools/gradient.h"
#include "tools/shapes.h"
#include "tools/stroke.h"

#include <oui/container.h>

#include <functional>
#include <string_view>

namespace olagarro::oui
{
class CheckButton;
class ComboBox;
class NumberField;
class Row;
class Slider;
} // namespace olagarro::oui

namespace oui_paint
{

// The bar of the tool in use's options, as Paint.NET's tool bar: the brush's width, hardness and antialiasing, the
// fill's mode, tolerance and sampling, the gradient's kind and mode, a line's arrowheads, a shape's kind and style;
// each tool shows those it uses.
class ToolOptions final : public olagarro::oui::Container
{
  public:
    explicit ToolOptions(BrushSettings settings);

    // Shows the options of the tool called `tool`.
    void show_for(std::string_view tool);

    [[nodiscard]] const BrushSettings& settings() const { return m_settings; }
    [[nodiscard]] FillSettings fill() const;
    [[nodiscard]] GradientKind gradient() const;
    [[nodiscard]] bool gradient_transparency() const;
    [[nodiscard]] LineSettings line() const;
    [[nodiscard]] ShapeSettings shape() const;
    // Width by `step` pixels, kept from 1 to the widest, as [ and ] do.
    void change_width(double step);
    // Called after a brush option changed.
    void on_changed(std::function<void(const BrushSettings&)> changed);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;

  private:
    void changed();

    BrushSettings m_settings;
    olagarro::oui::Row* m_width_group{};
    olagarro::oui::Row* m_hardness_group{};
    olagarro::oui::Row* m_antialias_group{};
    olagarro::oui::Row* m_fill_group{};
    olagarro::oui::Row* m_gradient_group{};
    olagarro::oui::Row* m_line_group{};
    olagarro::oui::Row* m_shape_group{};
    olagarro::oui::NumberField* m_width{};
    olagarro::oui::Slider* m_width_slider{};
    olagarro::oui::NumberField* m_hardness{};
    olagarro::oui::CheckButton* m_antialias{};
    olagarro::oui::ComboBox* m_flood_mode{};
    olagarro::oui::NumberField* m_tolerance{};
    olagarro::oui::ComboBox* m_sampling{};
    olagarro::oui::ComboBox* m_gradient_kind{};
    olagarro::oui::ComboBox* m_gradient_mode{};
    olagarro::oui::CheckButton* m_arrow_start{};
    olagarro::oui::CheckButton* m_arrow_end{};
    olagarro::oui::ComboBox* m_shape_kind{};
    olagarro::oui::ComboBox* m_shape_style{};
    std::function<void(const BrushSettings&)> m_changed;
    bool m_updating{false};
};

} // namespace oui_paint

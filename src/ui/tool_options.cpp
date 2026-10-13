#include "ui/tool_options.h"

#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/slider.h>
#include <oui/theme.h>

#include <memory>
#include <string>
#include <vector>

using namespace olagarro;

namespace oui_paint
{

namespace
{

constexpr double widest{2000.0};

oui::Row& group(oui::Container& parent)
{
    auto& row{oui::row(parent)};
    row.set_gap(oui::Spacing::tight);
    return row;
}

oui::ComboBox& choice(oui::Row& row, std::string_view label, std::vector<std::string> options)
{
    auto& name{oui::label(label, row)};
    auto& combo{row.add(std::make_unique<oui::ComboBox>(std::move(options), 0))};
    name.set_buddy(combo);
    return combo;
}

} // namespace

ToolOptions::ToolOptions(BrushSettings settings) : m_settings{settings}
{
    auto& bar{oui::row(*this)};
    bar.set_gap(oui::Spacing::normal);

    m_width_group = &group(bar);
    auto& width_label{oui::label("Brush &width:", *m_width_group)};
    m_width = &oui::number_field(*m_width_group);
    m_width->set_range(1, widest);
    m_width->set_value(m_settings.width);
    width_label.set_buddy(m_width->text_field());
    m_width_slider = &oui::slider(*m_width_group);
    m_width_slider->set_range(1, 200);
    m_width_slider->set_value(m_settings.width);

    m_hardness_group = &group(bar);
    auto& hardness_label{oui::label("&Hardness:", *m_hardness_group)};
    m_hardness = &oui::number_field(*m_hardness_group);
    m_hardness->set_range(0, 100);
    m_hardness->set_value(m_settings.hardness * 100.0);
    hardness_label.set_buddy(m_hardness->text_field());
    oui::label("%", *m_hardness_group);

    m_antialias_group = &group(bar);
    m_antialias = &oui::check_button("&Antialiasing", *m_antialias_group);
    m_antialias->set_checked(m_settings.antialias);

    m_fill_group = &group(bar);
    m_flood_mode = &choice(*m_fill_group, "&Flood mode:", {"Contiguous", "Global"});
    auto& tolerance_label{oui::label("&Tolerance:", *m_fill_group)};
    m_tolerance = &oui::number_field(*m_fill_group);
    m_tolerance->set_range(0, 100);
    m_tolerance->set_value(50);
    tolerance_label.set_buddy(m_tolerance->text_field());
    oui::label("%", *m_fill_group);
    m_sampling = &choice(*m_fill_group, "&Sampling:", {"Layer", "Image"});

    m_gradient_group = &group(bar);
    std::vector<std::string> kinds;
    for(const auto kind : all_gradient_kinds())
    {
        kinds.emplace_back(gradient_name(kind));
    }
    m_gradient_kind = &choice(*m_gradient_group, "&Gradient:", kinds);
    m_gradient_mode = &choice(*m_gradient_group, "&Mode:", {"Color", "Transparency"});

    m_line_group = &group(bar);
    m_arrow_start = &oui::check_button("Arrow at &start", *m_line_group);
    m_arrow_end = &oui::check_button("Arrow at &end", *m_line_group);

    m_shape_group = &group(bar);
    m_shape_kind = &choice(*m_shape_group, "&Shape:", {"Rectangle", "Rounded Rectangle", "Ellipse"});
    m_shape_style = &choice(*m_shape_group, "St&yle:", {"Outline", "Filled", "Filled with Outline"});

    static_cast<void>(m_width->on_changed(
        [this](double value)
        {
            m_settings.width = value;
            if(!m_updating)
            {
                m_updating = true;
                m_width_slider->set_value(value);
                m_updating = false;
            }
            changed();
        }));
    static_cast<void>(m_width_slider->on_changed(
        [this](double value)
        {
            if(!m_updating)
            {
                m_updating = true;
                m_width->set_value(value);
                m_updating = false;
            }
        }));
    static_cast<void>(m_hardness->on_changed(
        [this](double value)
        {
            m_settings.hardness = value / 100.0;
            changed();
        }));
    static_cast<void>(m_antialias->on_toggled(
        [this](bool checked)
        {
            m_settings.antialias = checked;
            changed();
        }));
    show_for("brush");
}

void ToolOptions::show_for(std::string_view tool)
{
    const auto brushing{tool == "brush" || tool == "eraser"};
    const auto drawing{tool == "line" || tool == "shapes"};
    m_width_group->set_visible(brushing || drawing || tool == "clone_stamp" || tool == "recolor");
    m_hardness_group->set_visible(brushing || tool == "clone_stamp" || tool == "recolor");
    m_antialias_group->set_visible(brushing || drawing || tool == "ellipse_select");
    m_fill_group->set_visible(tool == "bucket" || tool == "magic_wand");
    m_gradient_group->set_visible(tool == "gradient");
    m_line_group->set_visible(tool == "line");
    m_shape_group->set_visible(tool == "shapes");
}

FillSettings ToolOptions::fill() const
{
    return {m_flood_mode->selected() == 1, m_tolerance->value() / 100.0, m_sampling->selected() == 1};
}

GradientKind ToolOptions::gradient() const
{
    return all_gradient_kinds()[m_gradient_kind->selected()];
}

bool ToolOptions::gradient_transparency() const
{
    return m_gradient_mode->selected() == 1;
}

LineSettings ToolOptions::line() const
{
    return {m_arrow_start->is_checked(), m_arrow_end->is_checked()};
}

ShapeSettings ToolOptions::shape() const
{
    return {static_cast<ShapeKind>(m_shape_kind->selected()), static_cast<ShapeStyle>(m_shape_style->selected())};
}

void ToolOptions::change_width(double step)
{
    m_width->set_value(m_settings.width + step);
}

void ToolOptions::on_changed(std::function<void(const BrushSettings&)> changed)
{
    m_changed = std::move(changed);
}

void ToolOptions::changed()
{
    if(m_changed)
    {
        m_changed(m_settings);
    }
}

oui::Size ToolOptions::measure(const oui::Constraints& constraints) const
{
    return {constraints.max_width, 1};
}

void ToolOptions::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    child_at(0).arrange(bounds);
}

void ToolOptions::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, theme.status_background);
    Container::paint(painter, theme);
}

} // namespace oui_paint

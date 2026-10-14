#include "ui/tool_options.h"

#include "ui/shown_names.h"

#include <oui/translation.h>
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

ToolOptions::ToolOptions(BrushSettings settings, std::vector<FontFamily> fonts)
    : m_settings{settings}, m_fonts{std::move(fonts)}
{
    auto& bar{oui::row(*this)};
    bar.set_gap(oui::Spacing::normal);

    m_width_group = &group(bar);
    auto& width_label{oui::label(oui::translate("Tools", "Brush &width:"), *m_width_group)};
    m_width = &oui::number_field(*m_width_group);
    m_width->set_range(1, widest);
    m_width->set_value(m_settings.width);
    width_label.set_buddy(m_width->text_field());
    m_width_slider = &oui::slider(*m_width_group);
    m_width_slider->set_range(1, 200);
    m_width_slider->set_value(m_settings.width);

    m_hardness_group = &group(bar);
    auto& hardness_label{oui::label(oui::translate("Tools", "&Hardness:"), *m_hardness_group)};
    m_hardness = &oui::number_field(*m_hardness_group);
    m_hardness->set_range(0, 100);
    m_hardness->set_value(m_settings.hardness * 100.0);
    hardness_label.set_buddy(m_hardness->text_field());
    oui::label("%", *m_hardness_group);

    m_antialias_group = &group(bar);
    m_antialias = &oui::check_button(oui::translate("Tools", "&Antialiasing"), *m_antialias_group);
    m_antialias->set_checked(m_settings.antialias);

    m_fill_group = &group(bar);
    m_flood_mode = &choice(*m_fill_group, oui::translate("Tools", "&Flood mode:"), {oui::translate("Tools", "Contiguous"), oui::translate("Tools", "Global")});
    auto& tolerance_label{oui::label(oui::translate("Tools", "&Tolerance:"), *m_fill_group)};
    m_tolerance = &oui::number_field(*m_fill_group);
    m_tolerance->set_range(0, 100);
    m_tolerance->set_value(50);
    tolerance_label.set_buddy(m_tolerance->text_field());
    oui::label("%", *m_fill_group);
    m_sampling = &choice(*m_fill_group, oui::translate("Tools", "&Sampling:"), {oui::translate("Tools", "Layer"), oui::translate("Tools", "Image")});

    m_gradient_group = &group(bar);
    std::vector<std::string> kinds;
    for(const auto kind : all_gradient_kinds())
    {
        kinds.push_back(shown_name(kind));
    }
    m_gradient_kind = &choice(*m_gradient_group, oui::translate("Tools", "&Gradient:"), kinds);
    m_gradient_mode = &choice(*m_gradient_group, oui::translate("Tools", "&Mode:"), {oui::translate("Tools", "Color"), oui::translate("Tools", "Transparency")});

    m_line_group = &group(bar);
    m_arrow_start = &oui::check_button(oui::translate("Tools", "Arrow at &start"), *m_line_group);
    m_arrow_end = &oui::check_button(oui::translate("Tools", "Arrow at &end"), *m_line_group);

    m_shape_group = &group(bar);
    m_shape_kind = &choice(*m_shape_group, oui::translate("Tools", "&Shape:"), {oui::translate("Tools", "Rectangle"), oui::translate("Tools", "Rounded Rectangle"), oui::translate("Tools", "Ellipse")});
    m_shape_style = &choice(*m_shape_group, oui::translate("Tools", "St&yle:"), {oui::translate("Tools", "Outline"), oui::translate("Tools", "Filled"), oui::translate("Tools", "Filled with Outline")});

    m_text_group = &group(bar);
    std::vector<std::string> names;
    std::size_t chosen{};
    for(const auto& family : m_fonts)
    {
        // A plain sans serif to start with, as Paint.NET starts with its system's.
        if(family.name == "DejaVu Sans" || family.name == "Noto Sans" || family.name == "Arial")
        {
            chosen = names.size();
        }
        names.push_back(family.name);
    }
    if(names.empty())
    {
        names.emplace_back("(no fonts)");
    }
    m_font = &choice(*m_text_group, oui::translate("Tools", "&Font:"), names);
    m_font->set_selected(chosen);
    auto& size_label{oui::label(oui::translate("Tools", "Si&ze:"), *m_text_group)};
    m_font_size = &oui::number_field(*m_text_group);
    m_font_size->set_range(4, 1000);
    m_font_size->set_value(32);
    size_label.set_buddy(m_font_size->text_field());

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
    m_antialias_group->set_visible(brushing || drawing || tool == "ellipse_select" || tool == "text");
    m_text_group->set_visible(tool == "text");
    m_fill_group->set_visible(tool == "bucket" || tool == "magic_wand" || tool == "recolor");
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

const FontFamily* ToolOptions::font() const
{
    return m_fonts.empty() ? nullptr : &m_fonts[m_font->selected()];
}

double ToolOptions::font_size() const
{
    return m_font_size->value();
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

#include "ui/tool_options.h"

#include <oui/check_button.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/slider.h>
#include <oui/theme.h>

using namespace olagarro;

namespace oui_paint
{

namespace
{

constexpr double widest{2000.0};

} // namespace

ToolOptions::ToolOptions(BrushSettings settings) : m_settings{settings}
{
    auto& row{oui::row(*this)};
    row.set_gap(oui::Spacing::tight);
    auto& width_label{oui::label("Brush &width:", row)};
    m_width = &oui::number_field(row);
    m_width->set_range(1, widest);
    m_width->set_value(m_settings.width);
    width_label.set_buddy(m_width->text_field());
    m_width_slider = &oui::slider(row);
    m_width_slider->set_range(1, 200);
    m_width_slider->set_value(m_settings.width);
    auto& hardness_label{oui::label("&Hardness:", row)};
    m_hardness = &oui::number_field(row);
    m_hardness->set_range(0, 100);
    m_hardness->set_value(m_settings.hardness * 100.0);
    hardness_label.set_buddy(m_hardness->text_field());
    oui::label("%", row);
    m_antialias = &oui::check_button("&Antialiasing", row);
    m_antialias->set_checked(m_settings.antialias);

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

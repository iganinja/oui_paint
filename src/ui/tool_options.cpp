#include "ui/tool_options.h"

#include "ui/color_panel.h"
#include "ui/shown_names.h"

#include <oui/translation.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/command.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/slider.h>
#include <oui/theme.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

using namespace olagarro;

namespace oui_paint
{

namespace
{

constexpr double widest{2000.0};

// Its one child no wider than `columns`, as a list of fonts whose longest name would push the rest of the bar off the
// window.
class Capped final : public oui::Container
{
  public:
    explicit Capped(int columns) : m_columns{columns} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        auto size{child_at(0).measure(constraints)};
        size.width = std::min(size.width, m_columns);
        return size;
    }

    void arrange(const oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        child_at(0).arrange(bounds);
    }

  private:
    int m_columns;
};

// Buttons that stay pressed, side by side, each an icon, as a word processor's bold and italic: a click presses one or
// lets it go, and `changed` hears it.
class ToggleIcons final : public oui::Widget
{
  public:
    struct Toggle
    {
        std::string icon;
        std::string tooltip;
        bool* pressed;
    };

    ToggleIcons(Icons& icons, std::vector<Toggle> toggles, std::function<void()> changed)
        : m_icons{icons}, m_toggles{std::move(toggles)}, m_changed{std::move(changed)}
    {
    }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({static_cast<int>(m_toggles.size()) * button_width, 1});
    }

    void paint(oui::Painter& painter, const oui::Theme& theme) const override
    {
        for(std::size_t index{}; index < m_toggles.size(); ++ index)
        {
            const oui::Rect bounds{m_bounds.x + static_cast<int>(index) * button_width, m_bounds.y, button_width, 1};
            if(*m_toggles[index].pressed)
            {
                painter.fill_rect(bounds, theme.menu_selected_background);
            }
            const auto area{painter.pixel_bounds(bounds)};
            if(!area)
            {
                continue;
            }
            const auto over{*m_toggles[index].pressed ? std::optional<oui::Color>{theme.menu_selected_background}
                                                      : std::nullopt};
            if(const auto* const picture{m_icons.picture(m_toggles[index].icon, area->width, area->height, over)})
            {
                oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
                image.alpha = true;
                painter.draw_image(bounds, image, theme);
            }
        }
    }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr || !mouse->pressed || mouse->right)
        {
            return false;
        }
        const auto index{static_cast<std::size_t>((mouse->x - m_bounds.x) / button_width)};
        if(mouse->x < m_bounds.x || index >= m_toggles.size())
        {
            return false;
        }
        *m_toggles[index].pressed = !*m_toggles[index].pressed;
        m_changed();
        return true;
    }

    [[nodiscard]] std::optional<std::string> tooltip_at(oui::Point point) const override
    {
        const auto index{static_cast<std::size_t>((point.x - m_bounds.x) / button_width)};
        if(point.x < m_bounds.x || index >= m_toggles.size())
        {
            return std::nullopt;
        }
        return m_toggles[index].tooltip;
    }

  private:
    static constexpr int button_width{2};

    Icons& m_icons;
    std::vector<Toggle> m_toggles;
    std::function<void()> m_changed;
};

// A button that is an icon, as the bar's sun: a click on it calls `clicked`.
class IconButton final : public oui::Widget
{
  public:
    IconButton(Icons& icons, std::string icon, std::string tooltip, std::function<void()> clicked)
        : m_icons{icons}, m_icon{std::move(icon)}, m_tooltip{std::move(tooltip)}, m_clicked{std::move(clicked)}
    {
    }

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({2, 1});
    }

    void paint(oui::Painter& painter, const oui::Theme& theme) const override
    {
        const auto area{painter.pixel_bounds(m_bounds)};
        if(!area)
        {
            return;
        }
        if(const auto* const picture{m_icons.picture(m_icon, area->width, area->height)})
        {
            oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
            image.alpha = true;
            painter.draw_image(m_bounds, image, theme);
        }
    }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr || !mouse->pressed || mouse->right)
        {
            return false;
        }
        m_clicked();
        return true;
    }

    [[nodiscard]] std::optional<std::string> tooltip_at(oui::Point) const override { return m_tooltip; }

  private:
    Icons& m_icons;
    std::string m_icon;
    std::string m_tooltip;
    std::function<void()> m_clicked;
};

// A square of a colour; a click on it calls `clicked`, to choose another.
class Swatch final : public oui::Widget
{
  public:
    // `color` is shown as it is when painted, so it must outlive the square.
    Swatch(const Color& color, std::function<void()> clicked) : m_color{&color}, m_clicked{std::move(clicked)} {}

    [[nodiscard]] oui::Size measure(const oui::Constraints& constraints) const override
    {
        return constraints.constrain({2, 1});
    }

    void paint(oui::Painter& painter, const oui::Theme& theme) const override
    {
        painter.fill_rect(m_bounds, on_screen(*m_color));
        painter.draw_field_frame(m_bounds, false, theme);
    }

    bool handle_event(const oui::Event& event) override
    {
        const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
        if(mouse == nullptr || !mouse->pressed || mouse->right)
        {
            return false;
        }
        m_clicked();
        return true;
    }

  private:
    const Color* m_color;
    std::function<void()> m_clicked;
};

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

ToolOptions::ToolOptions(BrushSettings settings, std::vector<FontFamily> fonts, Icons& icons,
                         ColorChooser choose_color, LightChooser choose_light)
    : m_settings{settings}, m_fonts{std::move(fonts)}, m_choose_color{std::move(choose_color)},
      m_choose_light{std::move(choose_light)}
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
        names.push_back(oui::translate("Tools", "(no fonts)"));
    }
    auto& font_label{oui::label(oui::translate("Tools", "&Font:"), *m_text_group)};
    auto& capped{m_text_group->add(std::make_unique<Capped>(14))};
    m_font = &capped.add(std::make_unique<oui::ComboBox>(std::move(names), 0));
    font_label.set_buddy(*m_font);
    m_font->set_selected(chosen);
    auto& size_label{oui::label(oui::translate("Tools", "Si&ze:"), *m_text_group)};
    // Three figures and the arrows: the text's options fit a narrow window.
    m_font_size = &oui::number_field(m_text_group->add(std::make_unique<Capped>(7)));
    m_font_size->set_range(4, 1000);
    m_font_size->set_value(32);
    size_label.set_buddy(m_font_size->text_field());
    m_text_group->add(std::make_unique<ToggleIcons>(
        icons,
        std::vector<ToggleIcons::Toggle>{{"Bold", oui::translate("Tools", "Bold"), &m_text_style[0]},
                                         {"Italic", oui::translate("Tools", "Italic"), &m_text_style[1]},
                                         {"Strikethrough", oui::translate("Tools", "Strikethrough"), &m_text_style[2]}},
        [this] { text_changed(); }));
    // What goes around text, in pixels, each off at 0, and its colour in a square beside it: a click on it asks for
    // another, and the text shows each as it is chosen. Beside the bar where it fits, under it where it does not.
    m_effects_group = &group(*this);
    const auto effect{[this](std::string_view name, double most, std::size_t which)
                      {
                          auto& label{oui::label(name, *m_effects_group)};
                          auto& field{oui::number_field(m_effects_group->add(std::make_unique<Capped>(6)))};
                          field.set_range(0, most);
                          field.set_value(0);
                          label.set_buddy(field.text_field());
                          static_cast<void>(field.on_changed([this](double) { text_changed(); }));
                          auto& color{m_effect_colors[which]};
                          // The colour dialog's title is the option's name, without its colon.
                          auto title{oui::strip_mnemonic(name)};
                          if(!title.empty() && title.back() == ':')
                          {
                              title.pop_back();
                          }
                          m_effects_group->add(std::make_unique<Swatch>(
                              color,
                              [this, &color, title]
                              {
                                  const auto start{color};
                                  const auto put{[this, &color](const Color& chosen)
                                                 {
                                                     color = chosen;
                                                     text_changed();
                                                 }};
                                  put(m_choose_color(title, start, put).value_or(start));
                              }));
                          return &field;
                      }};
    m_outline = effect(oui::translate("Tools", "&Outline:"), 50, 0);
    m_shadow = effect(oui::translate("Tools", "S&hadow:"), 50, 1);
    // Where the light that casts the shadow comes from, asked for with the sun beside its colour.
    m_effects_group->add(std::make_unique<IconButton>(icons, "Light", oui::translate("Tools", "Light Direction"),
                                                      [this]
                                                      {
                                                          const auto start{m_light};
                                                          const auto put{[this](double degrees)
                                                                         {
                                                                             m_light = degrees;
                                                                             text_changed();
                                                                         }};
                                                          put(m_choose_light(start, put).value_or(start));
                                                      }));
    m_glow = effect(oui::translate("Tools", "&Glow:"), 100, 2);
    static_cast<void>(m_font->on_selected([this](std::size_t) { text_changed(); }));
    static_cast<void>(m_font_size->on_changed([this](double) { text_changed(); }));

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
            text_changed();
        }));
    show_for("brush");
}

void ToolOptions::show_for(std::string_view tool)
{
    const auto brushing{tool == "brush" || tool == "eraser"};
    const auto drawing{tool == "line" || tool == "shapes" || tool == "arc"};
    m_width_group->set_visible(brushing || drawing || tool == "clone_stamp" || tool == "recolor");
    m_hardness_group->set_visible(brushing || tool == "clone_stamp" || tool == "recolor");
    m_antialias_group->set_visible(brushing || drawing || tool == "ellipse_select" || tool == "text");
    m_text_group->set_visible(tool == "text");
    m_effects_group->set_visible(tool == "text");
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

TextEffects ToolOptions::text_effects() const
{
    TextEffects effects;
    effects.outline = static_cast<int>(m_outline->value());
    effects.shadow = static_cast<int>(m_shadow->value());
    effects.glow = static_cast<int>(m_glow->value());
    effects.outline_color = m_effect_colors[0];
    effects.shadow_color = m_effect_colors[1];
    effects.glow_color = m_effect_colors[2];
    effects.light = m_light;
    return effects;
}

ToolOptions::TextStyle ToolOptions::text_style() const
{
    return {m_text_style[0], m_text_style[1], m_text_style[2]};
}

void ToolOptions::change_width(double step)
{
    m_width->set_value(m_settings.width + step);
}

void ToolOptions::on_changed(std::function<void(const BrushSettings&)> changed)
{
    m_changed = std::move(changed);
}

void ToolOptions::on_text_changed(std::function<void()> changed)
{
    m_text_changed = std::move(changed);
}

void ToolOptions::text_changed()
{
    if(m_text_changed)
    {
        m_text_changed();
    }
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
    return {constraints.max_width, one_line(constraints.max_width) ? 1 : 2};
}

bool ToolOptions::one_line(int width) const
{
    if(!m_effects_group->is_visible())
    {
        return true;
    }
    const oui::Constraints loose{std::numeric_limits<int>::max() / 2, 1};
    return child_at(0).measure(loose).width + effects_gap + m_effects_group->measure(loose).width <= width;
}

void ToolOptions::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    auto& bar{child_at(0)};
    if(!m_effects_group->is_visible())
    {
        bar.arrange(bounds);
        m_effects_group->arrange({bounds.x, bounds.y, 0, 0});
        return;
    }
    const oui::Constraints loose{std::numeric_limits<int>::max() / 2, 1};
    if(one_line(bounds.width))
    {
        const auto width{bar.measure(loose).width};
        bar.arrange({bounds.x, bounds.y, width, 1});
        m_effects_group->arrange({bounds.x + width + effects_gap, bounds.y,
                                  bounds.width - width - effects_gap, 1});
        return;
    }
    bar.arrange({bounds.x, bounds.y, bounds.width, 1});
    m_effects_group->arrange({bounds.x, bounds.y + 1, bounds.width, 1});
}

void ToolOptions::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, theme.status_background);
    Container::paint(painter, theme);
}

} // namespace oui_paint

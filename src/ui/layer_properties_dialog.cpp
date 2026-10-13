#include "ui/layer_properties_dialog.h"

#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/slider.h>
#include <oui/window.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

std::optional<LayerProperties> ask_layer_properties(oui::Application& application, const LayerProperties& start)
{
    auto window{oui::window("Layer Properties")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);

    auto& name_label{oui::label("&Name:", form)};
    auto& name{oui::line_edit(form)};
    name.set_text(start.name);
    name_label.set_buddy(name);
    oui::label("", form);

    oui::label("", form);
    auto& visible{oui::check_button("&Visible", form)};
    visible.set_checked(start.visible);
    oui::label("", form);

    std::vector<std::string> modes;
    std::size_t selected{};
    for(const auto mode : all_blend_modes())
    {
        if(mode == start.blend_mode)
        {
            selected = modes.size();
        }
        modes.emplace_back(blend_mode_name(mode));
    }
    auto& mode_label{oui::label("&Blend mode:", form)};
    auto& mode{form.add(std::make_unique<oui::ComboBox>(modes, selected))};
    mode_label.set_buddy(mode);
    oui::label("", form);

    // Opacity from 0 to 255, as Paint.NET counts it.
    auto& opacity_label{oui::label("&Opacity:", form)};
    auto& opacity_slider{oui::slider(form)};
    opacity_slider.set_range(0, 255);
    auto& opacity{oui::number_field(form)};
    opacity.set_range(0, 255);
    opacity.set_value(std::round(start.opacity * 255.0));
    opacity_slider.set_value(opacity.value());
    opacity_label.set_buddy(opacity.text_field());
    static_cast<void>(opacity_slider.on_changed([&opacity](double value) { opacity.set_value(value); }));
    static_cast<void>(opacity.on_changed([&opacity_slider](double value) { opacity_slider.set_value(value); }));

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("OK", buttons,
                [&]
                {
                    opacity.commit();
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                });
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });

    application.focus(name);
    static_cast<void>(application.show_modal_and_keep(window));
    if(!accepted)
    {
        return std::nullopt;
    }
    return LayerProperties{std::string{name.text()}, visible.is_checked(), all_blend_modes()[mode.selected()],
                           opacity.value() / 255.0};
}

} // namespace oui_paint

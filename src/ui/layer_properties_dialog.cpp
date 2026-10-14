#include "ui/layer_properties_dialog.h"

#include "ui/shown_names.h"

#include <oui/translation.h>
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

std::optional<LayerProperties> ask_layer_properties(oui::Application& application, const LayerProperties& start,
                                                    const std::function<void(const LayerProperties&)>& changed)
{
    auto window{oui::window(oui::translate("Dialogs", "Layer Properties"))};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);

    auto& name_label{oui::label(oui::translate("Dialogs", "&Name:"), form)};
    auto& name{oui::line_edit(form)};
    name.set_text(start.name);
    name_label.set_buddy(name);
    oui::label("", form);

    oui::label("", form);
    auto& visible{oui::check_button(oui::translate("Dialogs", "&Visible"), form)};
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
        modes.push_back(shown_name(mode));
    }
    auto& mode_label{oui::label(oui::translate("Dialogs", "&Blend mode:"), form)};
    auto& mode{form.add(std::make_unique<oui::ComboBox>(modes, selected))};
    mode_label.set_buddy(mode);
    oui::label("", form);

    // Opacity from 0 to 255, as Paint.NET counts it.
    auto& opacity_label{oui::label(oui::translate("Dialogs", "&Opacity:"), form)};
    auto& opacity_slider{oui::slider(form)};
    opacity_slider.set_range(0, 255);
    auto& opacity{oui::number_field(form)};
    opacity.set_range(0, 255);
    opacity.set_value(std::round(start.opacity * 255.0));
    opacity_slider.set_value(opacity.value());
    opacity_label.set_buddy(opacity.text_field());
    // The properties as the dialog has them now, heard at every change.
    const auto now{[&]
                   {
                       return LayerProperties{std::string{name.text()}, visible.is_checked(),
                                              all_blend_modes()[mode.selected()], opacity.value() / 255.0};
                   }};
    static_cast<void>(opacity_slider.on_changed([&opacity](double value) { opacity.set_value(value); }));
    static_cast<void>(opacity.on_changed(
        [&](double value)
        {
            opacity_slider.set_value(value);
            changed(now());
        }));
    static_cast<void>(name.on_changed([&](std::string_view) { changed(now()); }));
    static_cast<void>(visible.on_toggled([&](bool) { changed(now()); }));
    static_cast<void>(mode.on_selected([&](std::size_t) { changed(now()); }));

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button(oui::translate("Dialogs", "OK"), buttons,
                [&]
                {
                    opacity.commit();
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button(oui::translate("Dialogs", "Cancel"), buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });

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

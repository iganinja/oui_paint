#include "ui/effect_dialog.h"

#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/slider.h>
#include <oui/window.h>

#include <memory>
#include <vector>

using namespace olagarro;

namespace oui_paint
{

bool ask_effect(oui::Application& application, const Effect& effect, std::map<std::string, double>& values,
                const std::function<void(const std::map<std::string, double>&)>& changed)
{
    auto window{oui::window(effect.name)};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::cells(24), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);
    std::vector<oui::NumberField*> fields;
    for(const auto& parameter : effect.parameters)
    {
        auto& label{oui::label(parameter.name + ":", form)};
        auto& value{values[parameter.id]};
        if(!parameter.options.empty())
        {
            auto& combo{form.add(std::make_unique<oui::ComboBox>(parameter.options,
                                                                 static_cast<std::size_t>(std::max(0.0, value))))};
            label.set_buddy(combo);
            oui::label("", form);
            static_cast<void>(combo.on_selected(
                [&values, &changed, id{parameter.id}](std::size_t index)
                {
                    values[id] = static_cast<double>(index);
                    changed(values);
                }));
            continue;
        }
        auto& slider{oui::slider(form)};
        slider.set_range(parameter.minimum, parameter.maximum);
        slider.set_step(parameter.step);
        slider.set_value(value);
        auto& field{oui::number_field(form)};
        field.set_range(parameter.minimum, parameter.maximum);
        field.set_step(parameter.step);
        field.set_decimals(parameter.step > 0.0 && parameter.step >= 1.0 ? 0 : 2);
        field.set_value(value);
        label.set_buddy(field.text_field());
        fields.push_back(&field);
        auto* const field_reference{&field};
        auto* const slider_reference{&slider};
        static_cast<void>(slider.on_changed([field_reference](double moved) { field_reference->set_value(moved); }));
        static_cast<void>(field.on_changed(
            [&values, &changed, slider_reference, id{parameter.id}](double typed)
            {
                slider_reference->set_value(typed);
                if(values[id] != typed)
                {
                    values[id] = typed;
                    changed(values);
                }
            }));
    }
    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("OK", buttons,
                [&]
                {
                    for(auto* const field : fields)
                    {
                        field->commit();
                    }
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                });
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    static_cast<void>(application.show_modal_and_keep(window));
    return accepted;
}

} // namespace oui_paint

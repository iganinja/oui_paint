#include "ui/effect_controls.h"

#include <oui/combo_box.h>
#include <oui/label.h>
#include <oui/line_edit.h>
#include <oui/slider.h>

#include <algorithm>
#include <memory>

using namespace olagarro;

namespace oui_paint
{

std::vector<oui::NumberField*> add_effect_controls(oui::Grid& form, const Effect& effect,
                                                   std::map<std::string, double>& values, std::function<void()> changed)
{
    std::vector<oui::NumberField*> fields;
    for(const auto& parameter : effect.parameters)
    {
        auto& label{oui::label(parameter.name + ":", form)};
        const auto value{values.try_emplace(parameter.id, parameter.value).first->second};
        if(!parameter.options.empty())
        {
            auto& combo{form.add(std::make_unique<oui::ComboBox>(parameter.options,
                                                                 static_cast<std::size_t>(std::max(0.0, value))))};
            label.set_buddy(combo);
            oui::label("", form);
            static_cast<void>(combo.on_selected(
                [&values, changed, id{parameter.id}](std::size_t index)
                {
                    values[id] = static_cast<double>(index);
                    changed();
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
            [&values, changed, slider_reference, id{parameter.id}](double typed)
            {
                slider_reference->set_value(typed);
                if(values[id] != typed)
                {
                    values[id] = typed;
                    changed();
                }
            }));
    }
    return fields;
}

} // namespace oui_paint

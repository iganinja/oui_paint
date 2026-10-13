#include "ui/effect_dialog.h"

#include "ui/curves_dialog.h"
#include "ui/effect_controls.h"
#include "ui/levels_dialog.h"

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
                const std::function<void(const std::map<std::string, double>&)>& changed, const Histogram& histogram)
{
    if(effect.dialog == "curves")
    {
        return ask_curves(application, values, changed, histogram);
    }
    if(effect.dialog == "levels")
    {
        return ask_levels(application, values, changed, histogram);
    }
    auto window{oui::window(effect.name)};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::cells(24), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);
    const auto fields{add_effect_controls(form, effect, values, [&values, &changed] { changed(values); })};
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

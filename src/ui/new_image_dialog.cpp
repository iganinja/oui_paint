#include "ui/new_image_dialog.h"

#include <oui/translation.h>
#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/window.h>

#include <string>
#include <type_traits>

using namespace olagarro;

namespace oui_paint
{

std::optional<NewImage> ask_new_image(oui::Application& application, const NewImage& start)
{
    auto window{oui::window(oui::translate("Dialogs", "New Image"))};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    // A label, what it names and what comes after it, a row each.
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);
    const auto field{[&form](std::string_view name, auto made, std::string_view after) -> auto&
                     {
                         auto& label{oui::label(name, form)};
                         auto& added{form.add(std::move(made))};
                         // A number field takes the focus in its text.
                         if constexpr(std::is_same_v<std::decay_t<decltype(added)>, oui::NumberField>)
                         {
                             label.set_buddy(added.text_field());
                         }
                         else
                         {
                             label.set_buddy(added);
                         }
                         oui::label(after, form);
                         return added;
                     }};
    auto& width{field(oui::translate("Dialogs", "&Width:"), oui::number_field(), oui::translate("Dialogs", "pixels"))};
    width.set_range(1, 65535);
    width.set_value(start.width);
    auto& height{field(oui::translate("Dialogs", "&Height:"), oui::number_field(), oui::translate("Dialogs", "pixels"))};
    height.set_range(1, 65535);
    height.set_value(start.height);
    auto& depth{field(oui::translate("Dialogs", "&Depth:"),
                      std::make_unique<oui::ComboBox>(
                          std::vector<std::string>{oui::translate("Dialogs", "8 bits per channel"), oui::translate("Dialogs", "16 bits per channel")}, start.bits == 16 ? 1u : 0u),
                      "")};
    auto& color{field(oui::translate("Dialogs", "&Color:"),
                      std::make_unique<oui::ComboBox>(std::vector<std::string>{oui::translate("Dialogs", "Color"), oui::translate("Dialogs", "Grayscale"), oui::translate("Dialogs", "Indexed")},
                                                      start.indexed ? 2u : start.gray ? 1u : 0u),
                      "")};
    // For an indexed image: how many palettes, of how many colours.
    auto& palettes{field(oui::translate("Dialogs", "&Palettes:"), oui::number_field(), oui::translate("Dialogs", "of"))};
    palettes.set_range(1, 64);
    palettes.set_value(start.palettes);
    auto& palette_size{field(oui::translate("Dialogs", "Colors &each:"), oui::number_field(), oui::translate("Dialogs", "colors"))};
    palette_size.set_range(2, 256);
    palette_size.set_value(start.palette_size);
    auto& background{field(oui::translate("Dialogs", "&Background:"),
                           std::make_unique<oui::ComboBox>(std::vector<std::string>{oui::translate("Dialogs", "White"), oui::translate("Dialogs", "Black"), oui::translate("Dialogs", "Transparent")},
                                                           static_cast<std::size_t>(start.background)),
                           "")};

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button(oui::translate("Dialogs", "OK"), buttons,
                [&]
                {
                    width.commit();
                    height.commit();
                    palettes.commit();
                    palette_size.commit();
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button(oui::translate("Dialogs", "Cancel"), buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });

    application.focus(width.text_field());
    width.text_field().select_all();
    // Kept, so its fields are still there to read once it closes.
    static_cast<void>(application.show_modal_and_keep(window));
    if(!accepted)
    {
        return std::nullopt;
    }
    NewImage result;
    result.width = static_cast<int>(width.value());
    result.height = static_cast<int>(height.value());
    result.bits = depth.selected() == 1 ? 16 : 8;
    result.gray = color.selected() == 1;
    result.indexed = color.selected() == 2;
    result.palettes = static_cast<int>(palettes.value());
    result.palette_size = static_cast<int>(palette_size.value());
    result.background = static_cast<Background>(background.selected());
    return result;
}

} // namespace oui_paint

#include "ui/preferences_dialog.h"

#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/translation.h>
#include <oui/window.h>

#include <algorithm>
#include <memory>

using namespace olagarro;

namespace oui_paint
{

std::optional<Preferences> ask_preferences(oui::Application& application, const Preferences& start,
                                           const std::vector<std::pair<std::string, std::string>>& languages,
                                           const std::vector<std::pair<std::string, std::string>>& themes,
                                           const std::function<void(const std::string& theme)>& theme_chosen)
{
    auto window{oui::window(oui::translate("Preferences", "Preferences"))};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);

    std::vector<std::string> names;
    std::size_t chosen{};
    for(const auto& [code, name] : languages)
    {
        if(code == start.language)
        {
            chosen = names.size();
        }
        names.push_back(name);
    }
    auto& language_label{oui::label(oui::translate("Preferences", "&Language:"), form)};
    auto& language{form.add(std::make_unique<oui::ComboBox>(names, chosen))};
    language_label.set_buddy(language);
    oui::label("", form);
    oui::label(oui::translate("Preferences", "The language changes when OUI Paint starts again."), form);

    std::vector<std::string> theme_names;
    std::size_t theme_chosen_index{};
    for(const auto& [id, name] : themes)
    {
        if(id == start.theme)
        {
            theme_chosen_index = theme_names.size();
        }
        theme_names.push_back(name);
    }
    auto& theme_label{oui::label(oui::translate("Preferences", "&Theme:"), form)};
    auto& theme{form.add(std::make_unique<oui::ComboBox>(theme_names, theme_chosen_index))};
    theme_label.set_buddy(theme);
    static_cast<void>(theme.on_selected([&](std::size_t index) { theme_chosen(themes.at(index).first); }));

    oui::label("", form);
    auto& reopen{oui::check_button(oui::translate("Preferences", "&Open the images of the last session at start"), form)};
    reopen.set_checked(start.reopen_last_session);
    oui::label("", form);
    auto& text_layer{oui::check_button(oui::translate("Preferences", "Put each text on a &new layer"), form)};
    text_layer.set_checked(start.text_on_new_layer);

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button(oui::translate("Dialogs", "OK"), buttons,
                [&]
                {
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button(oui::translate("Dialogs", "Cancel"), buttons,
                [&] { window_reference.close(oui::DialogResult::cancelled); });
    static_cast<void>(application.show_modal_and_keep(window));
    if(!accepted)
    {
        return std::nullopt;
    }
    Preferences chosen_preferences;
    chosen_preferences.language = languages.at(language.selected()).first;
    chosen_preferences.reopen_last_session = reopen.is_checked();
    chosen_preferences.text_on_new_layer = text_layer.is_checked();
    chosen_preferences.theme = themes.at(theme.selected()).first;
    return chosen_preferences;
}

} // namespace oui_paint

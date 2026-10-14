#include "ui/plugins_dialog.h"

#include <oui/button.h>
#include <oui/file_system.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/process.h>
#include <oui/table.h>
#include <oui/window.h>

using namespace olagarro;

namespace oui_paint
{

PluginsChoice ask_manage_plugins(oui::Application& application, const std::vector<PluginRecord>& plugins,
                                 const std::filesystem::path& folder)
{
    auto window{oui::window("Manage Plugins")};
    window->set_preferred_size({96, 0});
    window->set_height_share(0.6);
    window->set_resizable(true);
    auto& content{oui::grid(*window)};
    content.set_columns({oui::Length::fill()});
    content.set_rows({oui::Length::content(), oui::Length::fill(), oui::Length::content()});
    content.set_gap(oui::Spacing::tight);
    oui::label("Plugins are JavaScript files in " + oui::utf8_of(folder) +
                   ". Edit opens one beside an image to try it on.",
               content);
    auto& table{content.add(std::make_unique<oui::Table>())};
    table.set_headers({"Plugin", "From", "Effects"});
    table.set_column_width(0, 20);
    table.set_column_width(1, 9);
    table.set_column_width(2, 40);
    table.set_stretch_column(2);
    for(const auto& plugin : plugins)
    {
        std::string effects;
        for(const auto& effect : plugin.effects)
        {
            effects += (effects.empty() ? "" : ", ") + effect;
        }
        if(plugin.error)
        {
            effects = "Failed: " + plugin.error->substr(0, plugin.error->find('\n'));
        }
        table.add_line({plugin.name, plugin.file ? "Yours" : "Built in", effects});
    }
    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    PluginsChoice choice;
    auto& window_reference{*window};
    const auto choose{[&](PluginsChoice::Action action)
                      {
                          choice.action = action;
                          choice.plugin = table.selected_row().value_or(0);
                          window_reference.close(oui::DialogResult::accepted);
                      }};
    oui::button("&New", buttons, [&] { choose(PluginsChoice::Action::new_plugin); });
    auto& edit{oui::button("&Edit", buttons, [&] { choose(PluginsChoice::Action::edit); })};
    edit.set_enabled(!plugins.empty());
    static_cast<void>(table.on_row_activated([&](std::size_t) { choose(PluginsChoice::Action::edit); }));
    oui::button("Open &Folder", buttons,
                [&folder]
                {
                    std::error_code ignored;
                    std::filesystem::create_directories(folder, ignored);
                    static_cast<void>(oui::open_externally(oui::utf8_of(folder)));
                });
    oui::button("&Reload", buttons, [&] { choose(PluginsChoice::Action::reload); });
    oui::button("&Close", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    application.focus(table);
    static_cast<void>(application.show_modal(std::move(window)));
    return choice;
}

} // namespace oui_paint

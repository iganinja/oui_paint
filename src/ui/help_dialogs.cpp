#include "ui/help_dialogs.h"

#include <oui/button.h>
#include <oui/grid.h>
#include <oui/layout.h>
#include <oui/markdown_view.h>
#include <oui/process.h>
#include <oui/tab_view.h>
#include <oui/window.h>

#include <algorithm>

using namespace olagarro;

namespace oui_paint
{

namespace
{

// A window of reading: tall, resizable, as a manual page.
std::unique_ptr<oui::Window> reading_window(std::string_view title)
{
    auto window{oui::window(title)};
    // As wide as it goes whatever the page, or it would change width with the tab shown.
    window->set_preferred_size({110, 0});
    window->set_maximum_size({110, 1 << 15});
    window->set_height_share(0.9);
    window->set_minimum_size({40, 10});
    window->set_resizable(true);
    return window;
}

oui::MarkdownView& reading_view(oui::MarkdownView& view)
{
    view.set_background(&oui::Theme::reading_background);
    view.set_foreground(&oui::Theme::reading_foreground);
    return view;
}

void open_link(std::string_view link)
{
    if(link.rfind("http://", 0) == 0 || link.rfind("https://", 0) == 0)
    {
        static_cast<void>(oui::open_externally(link));
    }
}

} // namespace

void show_help(oui::Application& application, const std::vector<HelpPage>& pages, std::size_t first)
{
    auto window{reading_window("OUI Paint Help")};
    auto& tabs{oui::tab_view(*window)};
    tabs.set_closable(false);
    std::vector<oui::MarkdownView*> views;
    std::vector<olagarro::signal_slot::Connection> links;
    for(const auto& page : pages)
    {
        auto& view{reading_view(tabs.add_tab(page.title, oui::markdown_view(page.markdown)))};
        views.push_back(&view);
    }
    for(auto* const view : views)
    {
        links.push_back(view->on_link(
            [&](const std::string& link)
            {
                const auto place{help_place(pages, link)};
                if(!place)
                {
                    open_link(link);
                    return;
                }
                tabs.select(place->page);
                auto& shown{*views.at(place->page)};
                if(place->anchor.empty() || !shown.scroll_to_anchor(place->anchor))
                {
                    shown.scroll_to(0);
                }
                application.focus(shown);
            }));
    }
    const auto focus_shown{[&application, &views, &tabs] { application.focus(*views.at(tabs.selected_index())); }};
    const auto selection{tabs.on_selection_changed([&focus_shown](std::size_t) { focus_shown(); })};
    tabs.select(std::min(first, pages.size() - 1));
    application.post(focus_shown);
    static_cast<void>(application.show_modal(std::move(window)));
}

void show_about(oui::Application& application)
{
    auto window{oui::window("About OUI Paint")};
    window->set_preferred_size({84, 0});
    window->set_height_share(0.7);
    window->set_resizable(true);
    // The text takes what the buttons under it leave.
    auto& content{oui::grid(*window)};
    content.set_columns({oui::Length::fill()});
    content.set_rows({oui::Length::fill(), oui::Length::content()});
    content.set_gap(oui::Spacing::tight);
    auto& view{reading_view(content.add(oui::markdown_view(about_markdown())))};
    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    oui::button("Third-Party &Notices", buttons,
                [&application]
                {
                    auto notices{reading_window("Third-Party Notices")};
                    auto& shown{reading_view(notices->add(oui::markdown_view(third_party_notices())))};
                    application.post([&application, &shown] { application.focus(shown); });
                    static_cast<void>(application.show_modal(std::move(notices)));
                });
    auto& window_reference{*window};
    oui::button("&Close", buttons, [&window_reference] { window_reference.close(oui::DialogResult::cancelled); });
    application.post([&application, &view] { application.focus(view); });
    static_cast<void>(application.show_modal(std::move(window)));
}

} // namespace oui_paint

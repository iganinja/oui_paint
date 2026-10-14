#pragma once

#include <oui/application.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oui_paint
{

// What is chosen once for every image rather than for one.
struct Preferences
{
    // "auto" for the system's, or a language's code, as "en" or "es".
    std::string language{"auto"};
    // At start, with no images given, the ones open when OUI Paint last closed open again.
    bool reopen_last_session{true};
    // Each text the Text tool types goes on a new layer of its own, named after it.
    bool text_on_new_layer{false};
    // The look, by the id of one of OUI's graphical themes.
    std::string theme{"classic"};
};

// Asks for the preferences, starting from `start`; `languages` and `themes` are the ids and names to choose among,
// "auto" the first language. `theme_chosen` hears each theme chosen, so the window shows it at once. None when
// cancelled.
[[nodiscard]] std::optional<Preferences> ask_preferences(olagarro::oui::Application& application, const Preferences& start,
                                                         const std::vector<std::pair<std::string, std::string>>& languages,
                                                         const std::vector<std::pair<std::string, std::string>>& themes,
                                                         const std::function<void(const std::string& theme)>& theme_chosen);

} // namespace oui_paint

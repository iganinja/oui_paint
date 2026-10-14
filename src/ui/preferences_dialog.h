#pragma once

#include <oui/application.h>

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
};

// Asks for the preferences, starting from `start`; `languages` are the codes and names to choose among, "auto" first.
// None when cancelled.
[[nodiscard]] std::optional<Preferences> ask_preferences(olagarro::oui::Application& application, const Preferences& start,
                                                         const std::vector<std::pair<std::string, std::string>>& languages);

} // namespace oui_paint

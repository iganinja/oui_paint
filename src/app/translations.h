#pragma once

#include <oui/translation.h>

#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// OUI Paint's catalogs, one a language, built into the program; OUI's own come with the library.
[[nodiscard]] const std::vector<olagarro::oui::Translation>& paint_translations();

// Installs the language the settings name, "auto" for the system's, before anything shows a text; gives its code.
std::string install_paint_language(std::string_view setting);

// The text a built-in plugin shows, as an effect's name, menu or parameter, in the language installed: plugins are
// JavaScript, so their texts are in the catalog under "Plugins", found from the plugins themselves. A plugin of the
// user's own shows as it is written.
[[nodiscard]] std::string plugin_text(std::string_view text);

} // namespace oui_paint

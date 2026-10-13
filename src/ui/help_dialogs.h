#pragma once

#include "app/help.h"

#include <oui/application.h>

#include <vector>

namespace oui_paint
{

// The help, a tab for each page, starting at page `first`. A link to another page goes to it; one out of the help, as
// a web address, opens in the system's browser.
void show_help(olagarro::oui::Application& application, const std::vector<HelpPage>& pages, std::size_t first = 0);

// About OUI Paint: its version and license, with a button for the licenses of what it carries.
void show_about(olagarro::oui::Application& application);

} // namespace oui_paint

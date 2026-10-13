#pragma once

#include <oui/command.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// OUI Paint's version, as the build gives it: "0.1.0".
[[nodiscard]] std::string_view version();

// A page of the help: its tab's title, the file its links name it by, and its text.
struct HelpPage
{
    std::string title;
    std::string file;
    std::string markdown;
};

// The help's pages: the guide, every key, from the commands as they are bound, and how to write plugins.
[[nodiscard]] std::vector<HelpPage> help_pages(const olagarro::oui::CommandRegistry& commands);

// Where a link of the help leads among its pages: the page and the section after "#", if any. None for a link out of
// the help, as a web address.
struct HelpPlace
{
    std::size_t page{};
    std::string anchor;
};
[[nodiscard]] std::optional<HelpPlace> help_place(const std::vector<HelpPage>& pages, std::string_view link);

// What About says: the version, the license, and where the code is.
[[nodiscard]] std::string about_markdown();
// The licenses of everything the program carries, each under its name, as their licenses ask a copy to carry them.
[[nodiscard]] std::string third_party_notices();

} // namespace oui_paint

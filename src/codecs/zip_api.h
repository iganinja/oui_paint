#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// miniz behind a door of OUI Paint's own, so its warnings stay in the one file that holds it.
namespace oui_paint::zip_api
{

struct Entry
{
    std::string name;
    std::string bytes;
    // Kept as it is, not compressed: OpenRaster's mimetype must be, and PNG gains nothing.
    bool stored{false};
};

// A zip of the entries, in their order; none when miniz fails.
[[nodiscard]] std::optional<std::string> zip(const std::vector<Entry>& entries);
// One entry of a zip, none when the zip cannot be read or has no such entry.
[[nodiscard]] std::optional<std::string> unzip(std::string_view zip, std::string_view name);

} // namespace oui_paint::zip_api

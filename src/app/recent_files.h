#pragma once

#include <string>
#include <vector>

namespace oui_paint
{

// The files used last, the newest first: `path` put first, once, and no more than `most` kept.
[[nodiscard]] std::vector<std::string> with_recent(std::vector<std::string> recent, const std::string& path,
                                                   std::size_t most = 10);

// A folder short enough for a menu: the home folder as ~, and the middle left out past `most` characters.
[[nodiscard]] std::string short_folder(const std::string& folder, const std::string& home, std::size_t most = 40);

} // namespace oui_paint

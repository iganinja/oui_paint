#include "app/recent_files.h"

#include <algorithm>

namespace oui_paint
{

std::vector<std::string> with_recent(std::vector<std::string> recent, const std::string& path, std::size_t most)
{
    recent.erase(std::remove(recent.begin(), recent.end(), path), recent.end());
    recent.insert(recent.begin(), path);
    if(recent.size() > most)
    {
        recent.resize(most);
    }
    return recent;
}

std::string short_folder(const std::string& folder, const std::string& home, std::size_t most)
{
    auto result{folder};
    if(!home.empty() && result.rfind(home, 0) == 0 && (result.size() == home.size() || result[home.size()] == '/'))
    {
        result = "~" + result.substr(home.size());
    }
    if(result.size() > most && most > 3)
    {
        // Its start and its end, which name it best.
        const auto keep{most - 3};
        result = result.substr(0, keep / 3) + "..." + result.substr(result.size() - (keep - keep / 3));
    }
    return result;
}

} // namespace oui_paint

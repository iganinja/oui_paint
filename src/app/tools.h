#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A tool as the Tools panel shows it and its key picks it.
struct ToolInfo
{
    std::string id;
    std::string icon;
    std::string title;
    // Paint.NET's key: pressing it again goes to the next tool with the same key.
    char32_t key;
};

// Paint.NET's tools, in the order of its Tools window.
[[nodiscard]] const std::vector<ToolInfo>& all_tools();
// The tool `key` picks when `current` is in use: the first with that key, or the one after `current` among them, round
// again; none for a key no tool has.
[[nodiscard]] std::optional<std::string> tool_for_key(char32_t key, std::string_view current);

} // namespace oui_paint

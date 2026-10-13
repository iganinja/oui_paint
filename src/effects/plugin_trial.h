#pragma once

#include "effects/plugins.h"

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// What came of trying a plugin as it is being written: the effects it declares, the one tried and what it made,
// what it printed, and why it failed, when it did.
struct PluginTrial
{
    std::vector<Effect> effects;
    // Which of `effects` was tried; none when it declares none, or failed before declaring any.
    std::optional<std::size_t> tried;
    std::optional<Image> made;
    std::string log;
    std::optional<std::string> error;
    // The line of the plugin the error is on, from 1, when it says.
    std::optional<std::size_t> error_line;
};

// Runs `source`, as the plugin file `file_name`, in a host of its own, then its effect named `effect_name`, or its
// first when it has none by that name, on `image`: with `values` for the parameters they name and their own defaults
// for the rest. A run or a call taking longer than `limit` fails, as one that loops forever.
[[nodiscard]] PluginTrial try_plugin(std::string_view source, std::string_view file_name,
                                     std::string_view effect_name, const Image& image,
                                     const std::map<std::string, double>& values, const EffectContext& context,
                                     std::chrono::milliseconds limit);

// The line an error names in `file_name`, as "effects.js:12", from 1.
[[nodiscard]] std::optional<std::size_t> error_line_in(std::string_view error, std::string_view file_name);

// What a new plugin starts as: an effect with a parameter, to change into one's own.
[[nodiscard]] std::string_view new_plugin_source();

} // namespace oui_paint

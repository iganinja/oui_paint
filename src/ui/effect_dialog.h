#pragma once

#include "effects/plugins.h"

#include <oui/application.h>

#include <functional>
#include <map>
#include <string>

namespace oui_paint
{

// An effect's parameters to set, a slider and a field for each number and a list for each choice, starting from
// `values`; `changed` hears every change, so the image shows it. True when accepted, with `values` as they were left.
[[nodiscard]] bool ask_effect(olagarro::oui::Application& application, const Effect& effect,
                              std::map<std::string, double>& values,
                              const std::function<void(const std::map<std::string, double>&)>& changed);

} // namespace oui_paint

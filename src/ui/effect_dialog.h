#pragma once

#include "effects/curves.h"
#include "effects/plugins.h"

#include <oui/application.h>

#include <functional>
#include <map>
#include <string>

namespace oui_paint
{

// An effect's parameters to set, a slider and a field for each number and a list for each choice, starting from
// `values`, or the program's own dialog when the effect asks for one, as Curves and Levels do, over `histogram`, what
// it works on; `changed` hears every change, so the image shows it. True when accepted, with `values` as left.
[[nodiscard]] bool ask_effect(olagarro::oui::Application& application, const Effect& effect,
                              std::map<std::string, double>& values,
                              const std::function<void(const std::map<std::string, double>&)>& changed,
                              const Histogram& histogram);

} // namespace oui_paint

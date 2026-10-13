#pragma once

#include "effects/curves.h"

#include <oui/application.h>

#include <functional>
#include <map>
#include <string>

namespace oui_paint
{

// The Curves adjustment's dialog, as Paint.NET's: a transfer map of luminosity, or of red, green and blue, the ones
// ticked edited together, each a curve on a graph over the image's histogram. Its curves go in `values` as put_curve
// keeps them, "mode" saying the map; `changed` hears every change, so the image shows it. True when accepted.
[[nodiscard]] bool ask_curves(olagarro::oui::Application& application, std::map<std::string, double>& values,
                              const std::function<void(const std::map<std::string, double>&)>& changed,
                              const Histogram& histogram);

} // namespace oui_paint

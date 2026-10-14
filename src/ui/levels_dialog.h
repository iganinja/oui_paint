#pragma once

#include "effects/curves.h"

#include <oui/application.h>

#include <functional>
#include <map>
#include <string>

namespace oui_paint
{

// The Levels adjustment's dialog, as Paint.NET's: what goes in over the image's histogram, with markers for black,
// grey and white to drag, and what comes out over a gradient, with markers for black and white; their numbers beside,
// Auto to stretch what the histogram has from black to white. Red, green and blue ticked are edited together, each
// keeping levels of its own. The values are the Levels plugin's: inputLow, inputHigh, gamma, outputLow and outputHigh,
// and each colour's own, as inputLow_r; `changed` hears every change, so the image shows it. True when accepted.
[[nodiscard]] bool ask_levels(olagarro::oui::Application& application, std::map<std::string, double>& values,
                              const std::function<void(const std::map<std::string, double>&)>& changed,
                              const Histogram& histogram);

// Where the grey marker stands, from 0 to 1, for a gamma between input black and white, both from 0 to 255: where
// the level that comes out halfway goes in.
[[nodiscard]] double gamma_marker(double input_low, double input_high, double gamma);
// The gamma the grey marker at `place` stands for.
[[nodiscard]] double gamma_at(double input_low, double input_high, double place);
// What Auto stretches: the levels of the histogram's darkest and lightest pixels but a two-hundredth either end.
[[nodiscard]] std::pair<int, int> auto_levels(const std::array<double, 256>& histogram);

} // namespace oui_paint

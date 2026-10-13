#pragma once

#include "core/image.h"

namespace oui_paint
{

// A picture to try effects on, as the plugin workbench shows them: color bars, a gray ramp, hues from full to pale,
// lines one pixel apart, a soft-edged circle, noise, and a patch fading from opaque to see-through, so what an effect
// does to each shows. The same every time.
[[nodiscard]] Image test_card(int width = 512, int height = 384);

} // namespace oui_paint

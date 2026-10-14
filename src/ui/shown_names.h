#pragma once

#include "core/layer.h"
#include "core/pixel_format.h"
#include "core/transform.h"
#include "tools/gradient.h"

#include <string>

namespace oui_paint
{

// Names as the interface shows them, in the language installed. The names of the core, as blend_mode_name, stay as
// they are: plugins and files name things by them.
[[nodiscard]] std::string shown_name(BlendMode mode);
[[nodiscard]] std::string shown_name(Resampling resampling);
[[nodiscard]] std::string shown_name(GradientKind kind);
// What a format keeps, as "RGBA, 8 bits per channel".
[[nodiscard]] std::string shown_name(PixelFormat format);

} // namespace oui_paint

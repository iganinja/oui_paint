#pragma once

#include "app/new_image.h"

#include <oui/application.h>

#include <optional>

namespace oui_paint
{

// Asks for a new image's size, depth, colour or grey, and background, starting from `start`; none when cancelled.
[[nodiscard]] std::optional<NewImage> ask_new_image(olagarro::oui::Application& application, const NewImage& start);

} // namespace oui_paint

#pragma once

#include "tools/stroke.h"

#include <oui/application.h>

#include <optional>
#include <string>

namespace oui_paint
{

// Asks for a colour, as Paint.NET's Colors window: a square of saturation and value and a bar of hue, numbers for red,
// green, blue, alpha, hue, saturation and value, hexadecimal, and grey as a 16-bit value, for heightmaps; none when
// cancelled.
[[nodiscard]] std::optional<Color> ask_color(olagarro::oui::Application& application, const std::string& title,
                                             const Color& start);

} // namespace oui_paint

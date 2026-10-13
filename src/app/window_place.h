#pragma once

#include <oui/graphics/window_backend.h>
#include <oui/settings.h>

namespace oui_paint
{

// Where the window was left, its size and whether it was maximized, as the settings keep it; the window's own
// defaults for what they do not have.
[[nodiscard]] olagarro::oui::WindowGeometry window_place_from(const olagarro::oui::Settings& settings);
// Keeps where the window is, to open it there the next time.
void keep_window_place(olagarro::oui::Settings& settings, const olagarro::oui::WindowGeometry& geometry);

} // namespace oui_paint

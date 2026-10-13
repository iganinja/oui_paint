#pragma once

#include <oui/file_dialog.h>

#include <vector>

namespace oui_paint
{

// What the file dialogs list: images to open and save, palettes to load. OUI separates a filter's patterns with ';'.
[[nodiscard]] const std::vector<olagarro::oui::FileFilter>& image_filters();
[[nodiscard]] const std::vector<olagarro::oui::FileFilter>& palette_filters();

} // namespace oui_paint

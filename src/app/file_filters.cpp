#include "app/file_filters.h"

#include <oui/translation.h>

namespace oui_paint
{

const std::vector<olagarro::oui::FileFilter>& image_filters()
{
    static const std::vector<olagarro::oui::FileFilter> filters{
        {olagarro::oui::translate("Files", "Images (*.ora, *.png, *.jpg, *.bmp, *.tga, *.gif, *.r16)"),
         "*.ora;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif;*.r16;*.raw"},
        {olagarro::oui::translate("Files", "OpenRaster, with layers (*.ora)"), "*.ora"},
        {olagarro::oui::translate("Files", "PNG (*.png)"), "*.png"},
        {olagarro::oui::translate("Files", "JPEG (*.jpg)"), "*.jpg;*.jpeg"},
        {olagarro::oui::translate("Files", "BMP (*.bmp)"), "*.bmp"},
        {olagarro::oui::translate("Files", "TGA (*.tga)"), "*.tga"},
        {olagarro::oui::translate("Files", "Raw 16-bit heightmap (*.r16)"), "*.r16;*.raw"},
    };
    return filters;
}

const std::vector<olagarro::oui::FileFilter>& palette_filters()
{
    static const std::vector<olagarro::oui::FileFilter> filters{
        {olagarro::oui::translate("Files", "Palettes and images (*.pal, *.gpl, *.act, *.png, *.gif, *.bmp)"), "*.pal;*.gpl;*.act;*.png;*.gif;*.bmp;*.tga"},
        {olagarro::oui::translate("Files", "JASC palette (*.pal)"), "*.pal"},
        {olagarro::oui::translate("Files", "GIMP palette (*.gpl)"), "*.gpl"},
        {olagarro::oui::translate("Files", "Photoshop palette (*.act)"), "*.act"},
    };
    return filters;
}

} // namespace oui_paint

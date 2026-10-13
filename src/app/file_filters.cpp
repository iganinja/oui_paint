#include "app/file_filters.h"

namespace oui_paint
{

const std::vector<olagarro::oui::FileFilter>& image_filters()
{
    static const std::vector<olagarro::oui::FileFilter> filters{
        {"Images (*.ora, *.png, *.jpg, *.bmp, *.tga, *.gif, *.r16)",
         "*.ora;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif;*.r16;*.raw"},
        {"OpenRaster, with layers (*.ora)", "*.ora"},
        {"PNG (*.png)", "*.png"},
        {"JPEG (*.jpg)", "*.jpg;*.jpeg"},
        {"BMP (*.bmp)", "*.bmp"},
        {"TGA (*.tga)", "*.tga"},
        {"Raw 16-bit heightmap (*.r16)", "*.r16;*.raw"},
    };
    return filters;
}

const std::vector<olagarro::oui::FileFilter>& palette_filters()
{
    static const std::vector<olagarro::oui::FileFilter> filters{
        {"Palettes and images (*.pal, *.gpl, *.act, *.png, *.gif, *.bmp)", "*.pal;*.gpl;*.act;*.png;*.gif;*.bmp;*.tga"},
        {"JASC palette (*.pal)", "*.pal"},
        {"GIMP palette (*.gpl)", "*.gpl"},
        {"Photoshop palette (*.act)", "*.act"},
    };
    return filters;
}

} // namespace oui_paint

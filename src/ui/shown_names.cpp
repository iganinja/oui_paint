#include "ui/shown_names.h"

#include <oui/translation.h>

namespace oui_paint
{

namespace oui = olagarro::oui;

std::string shown_name(BlendMode mode)
{
    switch(mode)
    {
    case BlendMode::normal:
        return oui::translate("BlendModes", "Normal");
    case BlendMode::multiply:
        return oui::translate("BlendModes", "Multiply");
    case BlendMode::additive:
        return oui::translate("BlendModes", "Additive");
    case BlendMode::color_burn:
        return oui::translate("BlendModes", "Color Burn");
    case BlendMode::color_dodge:
        return oui::translate("BlendModes", "Color Dodge");
    case BlendMode::reflect:
        return oui::translate("BlendModes", "Reflect");
    case BlendMode::glow:
        return oui::translate("BlendModes", "Glow");
    case BlendMode::overlay:
        return oui::translate("BlendModes", "Overlay");
    case BlendMode::difference:
        return oui::translate("BlendModes", "Difference");
    case BlendMode::negation:
        return oui::translate("BlendModes", "Negation");
    case BlendMode::lighten:
        return oui::translate("BlendModes", "Lighten");
    case BlendMode::darken:
        return oui::translate("BlendModes", "Darken");
    case BlendMode::screen:
        return oui::translate("BlendModes", "Screen");
    case BlendMode::xor_:
        break;
    }
    return oui::translate("BlendModes", "Xor");
}

std::string shown_name(Resampling resampling)
{
    switch(resampling)
    {
    case Resampling::nearest:
        return oui::translate("Dialogs", "Nearest Neighbor");
    case Resampling::bilinear:
        return oui::translate("Dialogs", "Bilinear");
    case Resampling::bicubic:
        return oui::translate("Dialogs", "Bicubic");
    case Resampling::supersampling:
        break;
    }
    return oui::translate("Dialogs", "Supersampling");
}

std::string shown_name(GradientKind kind)
{
    switch(kind)
    {
    case GradientKind::linear:
        return oui::translate("Tools", "Linear");
    case GradientKind::linear_reflected:
        return oui::translate("Tools", "Linear Reflected");
    case GradientKind::diamond:
        return oui::translate("Tools", "Linear Diamond");
    case GradientKind::radial:
        return oui::translate("Tools", "Radial");
    case GradientKind::conical:
        break;
    }
    return oui::translate("Tools", "Conical");
}

std::string shown_name(PixelFormat format)
{
    switch(format)
    {
    case PixelFormat::gray_alpha8:
        return oui::translate("Status", "Grayscale, 8 bits per channel");
    case PixelFormat::rgba8:
        return oui::translate("Status", "RGBA, 8 bits per channel");
    case PixelFormat::gray_alpha16:
        return oui::translate("Status", "Grayscale, 16 bits per channel");
    case PixelFormat::rgba16:
        break;
    }
    return oui::translate("Status", "RGBA, 16 bits per channel");
}

} // namespace oui_paint

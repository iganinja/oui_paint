#include "app/tools.h"

#include <oui/translation.h>

namespace oui_paint
{

const std::vector<ToolInfo>& all_tools()
{
    static const std::vector<ToolInfo> tools{
        {"rectangle_select", "RegionEdit", olagarro::oui::translate("Tools", "Rectangle Select"), U's'},
        {"move_pixels", "ToolMove", olagarro::oui::translate("Tools", "Move Selected Pixels"), U'm'},
        {"lasso_select", "Lasso", olagarro::oui::translate("Tools", "Lasso Select"), U's'},
        {"move_selection", "ToolSelect", olagarro::oui::translate("Tools", "Move Selection"), U'm'},
        {"ellipse_select", "EllipseSelect", olagarro::oui::translate("Tools", "Ellipse Select"), U's'},
        {"zoom", "Zoom", olagarro::oui::translate("Tools", "Zoom"), U'z'},
        {"magic_wand", "MagicWand", olagarro::oui::translate("Tools", "Magic Wand"), U's'},
        {"pan", "ToolPan", olagarro::oui::translate("Tools", "Pan"), U'h'},
        {"bucket", "Bucket", olagarro::oui::translate("Tools", "Paint Bucket"), U'f'},
        {"gradient", "Gradient", olagarro::oui::translate("Tools", "Gradient"), U'g'},
        {"brush", "Paintbrush", olagarro::oui::translate("Tools", "Paintbrush"), U'b'},
        {"eraser", "Eraser", olagarro::oui::translate("Tools", "Eraser"), U'e'},
        {"pencil", "Edit", olagarro::oui::translate("Tools", "Pencil"), U'p'},
        {"color_picker", "ColorPick", olagarro::oui::translate("Tools", "Color Picker"), U'k'},
        {"clone_stamp", "CloneStamp", olagarro::oui::translate("Tools", "Clone Stamp"), U'l'},
        {"recolor", "Recolor", olagarro::oui::translate("Tools", "Recolor"), U'r'},
        {"text", "Text", olagarro::oui::translate("Tools", "Text"), U't'},
        {"line", "Line", olagarro::oui::translate("Tools", "Line / Curve"), U'o'},
        {"shapes", "Shapes", olagarro::oui::translate("Tools", "Shapes"), U'o'},
    };
    return tools;
}

std::optional<std::string> tool_for_key(char32_t key, std::string_view current)
{
    // The order the key goes through them is Paint.NET's: rectangle, lasso, ellipse, magic wand.
    std::vector<const ToolInfo*> sharing;
    for(const auto& tool : all_tools())
    {
        if(tool.key == key)
        {
            sharing.push_back(&tool);
        }
    }
    if(sharing.empty())
    {
        return std::nullopt;
    }
    for(std::size_t index{}; index < sharing.size(); ++ index)
    {
        if(sharing[index]->id == current)
        {
            return sharing[(index + 1) % sharing.size()]->id;
        }
    }
    return sharing.front()->id;
}

} // namespace oui_paint

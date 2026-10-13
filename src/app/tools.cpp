#include "app/tools.h"

namespace oui_paint
{

const std::vector<ToolInfo>& all_tools()
{
    static const std::vector<ToolInfo> tools{
        {"rectangle_select", "RegionEdit", "Rectangle Select", U's'},
        {"move_pixels", "ToolMove", "Move Selected Pixels", U'm'},
        {"lasso_select", "Lasso", "Lasso Select", U's'},
        {"move_selection", "ToolSelect", "Move Selection", U'm'},
        {"ellipse_select", "EllipseSelect", "Ellipse Select", U's'},
        {"zoom", "Zoom", "Zoom", U'z'},
        {"magic_wand", "MagicWand", "Magic Wand", U's'},
        {"pan", "ToolPan", "Pan", U'h'},
        {"bucket", "Bucket", "Paint Bucket", U'f'},
        {"gradient", "Gradient", "Gradient", U'g'},
        {"brush", "Paintbrush", "Paintbrush", U'b'},
        {"eraser", "Eraser", "Eraser", U'e'},
        {"pencil", "Edit", "Pencil", U'p'},
        {"color_picker", "ColorPick", "Color Picker", U'k'},
        {"clone_stamp", "CloneStamp", "Clone Stamp", U'l'},
        {"recolor", "Recolor", "Recolor", U'r'},
        {"text", "Text", "Text", U't'},
        {"line", "Line", "Line / Curve", U'o'},
        {"shapes", "Shapes", "Shapes", U'o'},
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

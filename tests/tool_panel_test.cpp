#include "ui/tool_panel.h"

#include <gtest/gtest.h>

using namespace olagarro;

namespace
{

std::vector<oui_paint::ToolButton> three_tools()
{
    return {{"select", "RegionEdit", "Select"}, {"move", "ToolMove", "Move"}, {"zoom", "Zoom", "Zoom"}};
}

} // namespace

TEST(ToolPanel, ButtonsGoTwoToARow)
{
    oui_paint::Icons icons;
    oui_paint::ToolPanel panel{three_tools(), icons};
    EXPECT_EQ(panel.measure({80, 25}).width, 4);
    EXPECT_EQ(panel.measure({80, 25}).height, 2);
    EXPECT_EQ(panel.button_at({0, 0}), 0u);
    EXPECT_EQ(panel.button_at({1, 0}), 0u);
    EXPECT_EQ(panel.button_at({2, 0}), 1u);
    EXPECT_EQ(panel.button_at({0, 1}), 2u);
    EXPECT_FALSE(panel.button_at({2, 1}).has_value());
    EXPECT_FALSE(panel.button_at({4, 0}).has_value());
}

TEST(ToolPanel, AClickPicksTheToolUnderIt)
{
    oui_paint::Icons icons;
    oui_paint::ToolPanel panel{three_tools(), icons};
    panel.arrange({10, 3, 4, 2});
    std::string picked;
    panel.on_picked([&picked](const std::string& id) { picked = id; });
    EXPECT_EQ(panel.selected(), "select");
    EXPECT_TRUE(panel.handle_event(oui::MouseEvent{10, 4, true}));
    EXPECT_EQ(picked, "zoom");
    EXPECT_EQ(panel.selected(), "zoom");
    EXPECT_EQ(panel.tooltip_at({12, 3}), "Move");
}

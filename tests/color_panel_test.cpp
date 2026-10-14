#include "ui/color_panel.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

olagarro::oui::MouseEvent click(int x, int y, bool right = false)
{
    olagarro::oui::MouseEvent mouse{x, y, true};
    mouse.right = right;
    return mouse;
}

} // namespace

TEST(ColorPanel, CChoosesWhichColorAClickOnThePaletteSets)
{
    ColorPanel panel;
    panel.arrange({0, 0, ColorPanel::width, 40});
    // The palette starts with black, then greys; the first colour is at the panel's fourth row.
    const auto first{*panel.palette_at({0, 3})};
    const auto second{*panel.palette_at({2, 3})};
    panel.set_primary({1, 0, 0, 1});
    panel.set_secondary({0, 0, 1, 1});
    EXPECT_FALSE(panel.editing_secondary());
    panel.handle_event(click(0, 3));
    EXPECT_EQ(panel.primary().red, first.red) << "a click sets the colour being edited, the primary";
    panel.toggle_editing();
    EXPECT_TRUE(panel.editing_secondary());
    panel.handle_event(click(2, 3));
    EXPECT_EQ(panel.secondary().red, second.red) << "now the secondary";
    EXPECT_EQ(panel.primary().red, first.red);
    panel.handle_event(click(2, 3, true));
    EXPECT_EQ(panel.primary().red, second.red) << "a right click sets the other";
}

TEST(ColorPanel, ItSaysEachTimeAColorChangesHoweverItChanges)
{
    ColorPanel panel;
    panel.arrange({0, 0, ColorPanel::width, 40});
    auto said{0};
    panel.on_changed([&said] { ++ said; });
    panel.set_primary({1, 0, 0, 1});
    panel.set_secondary({0, 0, 1, 1});
    panel.swap();
    panel.handle_event(click(0, 3));
    EXPECT_EQ(said, 4);
}

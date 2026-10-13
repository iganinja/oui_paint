#include "app/tools.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(Tools, AKeyPicksItsFirstToolAndAgainTheNextRoundAgain)
{
    EXPECT_EQ(tool_for_key(U's', "brush"), "rectangle_select");
    EXPECT_EQ(tool_for_key(U's', "rectangle_select"), "lasso_select");
    EXPECT_EQ(tool_for_key(U's', "lasso_select"), "ellipse_select");
    EXPECT_EQ(tool_for_key(U's', "ellipse_select"), "magic_wand");
    EXPECT_EQ(tool_for_key(U's', "magic_wand"), "rectangle_select");
    EXPECT_EQ(tool_for_key(U'm', "move_pixels"), "move_selection");
    EXPECT_EQ(tool_for_key(U'o', "line"), "shapes");
    EXPECT_EQ(tool_for_key(U'b', "brush"), "brush");
    EXPECT_FALSE(tool_for_key(U'q', "brush").has_value());
}

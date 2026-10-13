#include "effects/curve_edit.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(CurveEdit, APressAddsAPointThereAndADragMovesItBetweenItsNeighbours)
{
    CurveEdit edit;
    EXPECT_TRUE(edit.press(0.5, 0.7, false));
    ASSERT_EQ(edit.points().size(), 3u);
    EXPECT_EQ(edit.points()[1], (CurvePoint{0.5, 0.7}));
    EXPECT_EQ(edit.held(), std::optional<std::size_t>{1});
    EXPECT_TRUE(edit.drag(1.2, 0.3)) << "past the last point, but not far enough to take it away";
    EXPECT_NEAR(edit.points()[1].x, 1.0 - 1.0 / 255.0, 1e-12) << "kept before it";
    EXPECT_EQ(edit.points()[1].y, 0.3);
    edit.release();
    EXPECT_FALSE(edit.press(0.0, 0.01, false)) << "near the first point: takes it, adds nothing";
    EXPECT_EQ(edit.points().size(), 3u);
    EXPECT_TRUE(edit.drag(0.1, 0.2));
    EXPECT_EQ(edit.points().front(), (CurvePoint{0.1, 0.2})) << "the ends move too";
}

TEST(CurveEdit, APointGoesWithARightPressOrDraggedFarOffButTwoStay)
{
    CurveEdit edit;
    edit.set_points({{0, 0}, {0.5, 0.5}, {1, 1}});
    EXPECT_TRUE(edit.press(0.5, 0.5, true));
    EXPECT_EQ(edit.points().size(), 2u);
    EXPECT_FALSE(edit.press(0, 0, true)) << "two are the fewest";
    edit.set_points({{0, 0}, {0.5, 0.5}, {1, 1}});
    edit.press(0.5, 0.5, false);
    EXPECT_TRUE(edit.drag(0.5, 1.4)) << "far above the graph";
    EXPECT_EQ(edit.points().size(), 2u);
    EXPECT_TRUE(edit.drag(0.5, 0.9)) << "back on it";
    ASSERT_EQ(edit.points().size(), 3u);
    EXPECT_EQ(edit.points()[1], (CurvePoint{0.5, 0.9}));
}

#include "ui/light_dialog.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(LightDial, ItHearsEveryMoveOfADragNotOnlyThoseFromOneCellToAnother)
{
    // Otherwise a drag up or down its sides, within a column of cells, moves the sun only at each new row.
    const LightDial dial{135};
    EXPECT_TRUE(dial.takes_raw_pointer());
}

TEST(LightDial, AnAngleIsKeptBetweenMinus180And180)
{
    LightDial dial{270};
    EXPECT_DOUBLE_EQ(dial.degrees(), -90);
    dial.set_degrees(-190);
    EXPECT_DOUBLE_EQ(dial.degrees(), 170);
}

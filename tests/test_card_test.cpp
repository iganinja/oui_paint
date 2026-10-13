#include "effects/test_card.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(TestCard, HasPureColorsAFullGrayRampAndSeeThroughPixels)
{
    const auto card{test_card(512, 384)};
    ASSERT_EQ(card.width, 512);
    ASSERT_EQ(card.height, 384);
    const auto at{[&card](int x, int y) { return card.pixel<Rgba8>(x, y); }};
    EXPECT_EQ(at(10, 10).value, (std::array<std::uint8_t, 4>{255, 255, 255, 255})) << "the first bar is white";
    EXPECT_EQ(at(5 * 64 + 10, 10).value, (std::array<std::uint8_t, 4>{255, 0, 0, 255})) << "the sixth is red";
    EXPECT_EQ(at(0, 100).value[0], 0) << "the gray ramp starts black";
    EXPECT_EQ(at(511, 100).value[0], 255) << "and ends white";
    EXPECT_EQ(at(384, 383).alpha(), 0) << "the last patch fades from see-through";
    EXPECT_EQ(at(511, 383).alpha(), 255) << "to opaque";
    EXPECT_NE(at(0, 300).value[0], at(1, 300).value[0]) << "lines one pixel apart";
}

TEST(TestCard, IsTheSameEveryTime)
{
    EXPECT_EQ(test_card(64, 48).bytes, test_card(64, 48).bytes);
}

#include "tools/shapes.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

std::uint8_t alpha_at(const Layer& layer, int x, int y)
{
    return layer.read(x, y, 1, 1).pixel<Rgba8>(0, 0).alpha();
}

} // namespace

TEST(Shapes, AnOutlineIsItsWidthInsideTheEdgeAndAFilledShapeIsSolid)
{
    const Selection everything{100, 100};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_shape(layer, everything, ShapeKind::rectangle, ShapeStyle::outline, {10, 10}, {50, 40}, {1, 0, 0, 1}, {},
               3.0, false);
    EXPECT_EQ(alpha_at(layer, 10, 20), 255);
    EXPECT_EQ(alpha_at(layer, 12, 20), 255);
    EXPECT_EQ(alpha_at(layer, 14, 20), 0) << "past the outline's width";
    EXPECT_EQ(alpha_at(layer, 9, 20), 0) << "outside the edge";
    Layer filled{PixelFormat::rgba8, "Filled"};
    draw_shape(filled, everything, ShapeKind::ellipse, ShapeStyle::filled, {10, 10}, {50, 40}, {0, 0, 1, 1}, {}, 3.0,
               true);
    EXPECT_EQ(alpha_at(filled, 30, 25), 255);
    EXPECT_EQ(alpha_at(filled, 11, 11), 0);
}

TEST(Shapes, BothStylesFillWithTheSecondaryInsideThePrimaryOutline)
{
    const Selection everything{100, 100};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_shape(layer, everything, ShapeKind::rectangle, ShapeStyle::outline_and_fill, {10, 10}, {50, 40},
               {1, 0, 0, 1}, {0, 0, 1, 1}, 2.0, false);
    EXPECT_EQ(layer.read(10, 20, 1, 1).pixel<Rgba8>(0, 0).value[0], 255);
    EXPECT_EQ(layer.read(30, 25, 1, 1).pixel<Rgba8>(0, 0).value[2], 255);
}

TEST(Shapes, ALineCanEndInAnArrowhead)
{
    const Selection everything{200, 100};
    Layer plain{PixelFormat::rgba8, "Plain"};
    draw_line(plain, everything, {20, 50}, {180, 50}, {0, 0, 0, 1}, 2.0, false, {});
    Layer arrowed{PixelFormat::rgba8, "Arrow"};
    draw_line(arrowed, everything, {20, 50}, {180, 50}, {0, 0, 0, 1}, 2.0, false, {false, true});
    EXPECT_EQ(alpha_at(plain, 173, 52), 0);
    EXPECT_EQ(alpha_at(arrowed, 173, 52), 255) << "the head is wider than the line";
    EXPECT_EQ(alpha_at(arrowed, 100, 50), 255);
}

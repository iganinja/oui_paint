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

TEST(Shapes, AShapeLandsOnlyWhereSelectedAndLeavesUnselectedTilesUntouched)
{
    Selection selection{600, 300};
    selection.add_rectangle({250, 20, 20, 20}, SelectionMode::replace);
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_shape(layer, selection, ShapeKind::rectangle, ShapeStyle::filled, {0, 0}, {600, 300}, {1, 0, 0, 1}, {}, 1.0,
               false);
    EXPECT_EQ(alpha_at(layer, 255, 30), 255) << "selected, across a tile's edge from the next one";
    EXPECT_EQ(alpha_at(layer, 260, 30), 255);
    EXPECT_EQ(alpha_at(layer, 249, 30), 0) << "next to the selection";
    EXPECT_EQ(alpha_at(layer, 255, 45), 0);
    EXPECT_EQ(layer.tile({2, 0}), nullptr) << "a tile with nothing selected is not even made";
    EXPECT_EQ(layer.tile({0, 1}), nullptr);
}

TEST(Shapes, AnInkNotOpaqueIsMixedOverWhatIsThere)
{
    const Selection everything{20, 20};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_shape(layer, everything, ShapeKind::rectangle, ShapeStyle::filled, {0, 0}, {20, 20}, {0, 0, 1, 1}, {}, 1.0,
               false);
    draw_shape(layer, everything, ShapeKind::rectangle, ShapeStyle::filled, {0, 0}, {20, 20}, {1, 0, 0, 0.5}, {}, 1.0,
               false);
    const auto pixel{layer.read(10, 10, 1, 1).pixel<Rgba8>(0, 0)};
    EXPECT_NEAR(pixel.value[0], 128, 1);
    EXPECT_NEAR(pixel.value[2], 127, 1);
    EXPECT_EQ(pixel.alpha(), 255);
}

TEST(Shapes, ACurveGoesThroughItsEndsAndItsHandles)
{
    const std::array<Position, 4> through{Position{10.5, 50.5}, Position{40.5, 20.5}, Position{70.5, 20.5},
                                          Position{100.5, 50.5}};
    const auto along{curve_through(through, 8)};
    ASSERT_EQ(along.size(), 25u) << "eight between each two, the last end too";
    EXPECT_DOUBLE_EQ(along.front().x, 10.5);
    EXPECT_DOUBLE_EQ(along[8].x, 40.5) << "through the first handle";
    EXPECT_DOUBLE_EQ(along[16].y, 20.5) << "and the second";
    EXPECT_DOUBLE_EQ(along.back().x, 100.5);
    const Selection everything{120, 80};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_curve(layer, everything, through, {0, 0, 0, 1}, 3.0, false, {});
    EXPECT_EQ(alpha_at(layer, 40, 20), 255);
    EXPECT_EQ(alpha_at(layer, 55, 16), 255) << "between the handles, rounding over them";
    EXPECT_EQ(alpha_at(layer, 55, 50), 0) << "not along the straight line from end to end";
}

TEST(Shapes, ACurveWithItsHandlesOnTheLineIsTheStraightLine)
{
    const Selection everything{120, 40};
    Layer straight{PixelFormat::rgba8, "Straight"};
    draw_line(straight, everything, {10, 20}, {100, 20}, {0, 0, 0, 1}, 4.0, true, {});
    Layer curve{PixelFormat::rgba8, "Curve"};
    draw_curve(curve, everything, {Position{10, 20}, Position{40, 20}, Position{70, 20}, Position{100, 20}},
               {0, 0, 0, 1}, 4.0, true, {});
    for(int x{5}; x < 110; x += 7)
    {
        for(int y{15}; y < 26; ++ y)
        {
            EXPECT_NEAR(alpha_at(curve, x, y), alpha_at(straight, x, y), 1) << x << ", " << y;
        }
    }
}

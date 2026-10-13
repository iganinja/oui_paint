#include "tools/floating.h"
#include "tools/stroke.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

// A 4 by 2 piece at (10, 10) on a see-through 40 by 40 layer, each pixel its own shade of red.
struct Scene
{
    Scene() : layer{PixelFormat::rgba8, "Layer"}, selection{40, 40}
    {
        Image block{PixelFormat::rgba8, 4, 2};
        for(int y{}; y < 2; ++ y)
        {
            for(int x{}; x < 4; ++ x)
            {
                block.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(10 + y * 4 + x), 0, 0, 255}});
            }
        }
        layer.write(block, 10, 10);
        selection.add_rectangle({10, 10, 4, 2}, SelectionMode::replace);
        piece = lift_piece(layer, selection);
        base = layer;
        static_cast<void>(erase_selected(base, selection));
    }

    int red(int x, int y) const { return layer.read(x, y, 1, 1).pixel<Rgba8>(0, 0).value[0]; }
    int alpha(int x, int y) const { return layer.read(x, y, 1, 1).pixel<Rgba8>(0, 0).alpha(); }

    Layer layer;
    Selection selection;
    FloatingPiece piece;
    Layer base{PixelFormat::rgba8, "Base"};
};

} // namespace

TEST(Floating, MirroringAPieceKeepsEveryPixelAndItsSelection)
{
    Scene scene;
    PieceTransform mirror;
    mirror.scale_x = -1.0;
    place_piece(scene.layer, scene.base, scene.selection, scene.piece, mirror);
    EXPECT_EQ(scene.red(10, 10), 13);
    EXPECT_EQ(scene.red(13, 10), 10);
    EXPECT_EQ(scene.red(10, 11), 17);
    EXPECT_EQ(scene.selection.at(13, 11), 255);
    EXPECT_EQ(scene.selection.at(14, 11), 0);
}

TEST(Floating, AQuarterTurnStandsThePieceUpRoundItsMiddle)
{
    Scene scene;
    PieceTransform turn;
    turn.radians = 3.14159265358979323846 / 2.0;
    place_piece(scene.layer, scene.base, scene.selection, scene.piece, turn);
    // 4 wide and 2 tall round (12, 11) becomes 2 wide and 4 tall, from (11, 9) to (13, 13).
    EXPECT_EQ(scene.alpha(10, 10), 0);
    EXPECT_EQ(scene.alpha(11, 9), 255);
    EXPECT_EQ(scene.alpha(12, 12), 255);
    EXPECT_EQ(scene.alpha(13, 12), 0);
    // Clockwise, the bottom left pixel comes to the top left.
    EXPECT_EQ(scene.red(11, 9), 14);
    EXPECT_EQ(scene.selection.at(12, 12), 255);
    EXPECT_EQ(scene.selection.at(13, 11), 0);
}

TEST(Floating, ScaledTwiceAsBigThePieceCoversFourTimesTheRoom)
{
    Scene scene;
    PieceTransform bigger;
    bigger.scale_x = 2.0;
    bigger.scale_y = 2.0;
    place_piece(scene.layer, scene.base, scene.selection, scene.piece, bigger);
    const auto corners{piece_corners(scene.piece, bigger)};
    EXPECT_DOUBLE_EQ(corners[0].x, 8.0);
    EXPECT_DOUBLE_EQ(corners[2].y, 13.0);
    EXPECT_EQ(scene.alpha(8, 9), 255);
    EXPECT_EQ(scene.alpha(15, 12), 255);
    EXPECT_EQ(scene.alpha(16, 12), 0);
}

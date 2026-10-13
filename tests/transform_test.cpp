#include "core/transform.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

Image numbered(int width, int height)
{
    Image image{PixelFormat::gray_alpha8, width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            image.set_pixel(x, y, GrayAlpha8{{static_cast<std::uint8_t>(y * width + x), 255}});
        }
    }
    return image;
}

int value(const Image& image, int x, int y)
{
    return image.pixel<GrayAlpha8>(x, y).value[0];
}

} // namespace

TEST(Transform, FlipsAndQuarterTurnsMovePixelsExactly)
{
    const auto image{numbered(3, 2)};
    EXPECT_EQ(value(flipped(image, true), 0, 0), 2);
    EXPECT_EQ(value(flipped(image, false), 0, 0), 3);
    const auto turned{rotated(image, 1)};
    ASSERT_EQ(turned.width, 2);
    ASSERT_EQ(turned.height, 3);
    // Clockwise: the bottom left corner comes to the top left.
    EXPECT_EQ(value(turned, 0, 0), 3);
    EXPECT_EQ(value(turned, 1, 0), 0);
    EXPECT_EQ(rotated(rotated(image, 1), 3).bytes, image.bytes);
    EXPECT_EQ(rotated(image, 2).bytes, flipped(flipped(image, true), false).bytes);
}

TEST(Transform, ResizingTwiceAsBigWithNearestRepeatsEachPixel)
{
    const auto image{numbered(2, 2)};
    const auto bigger{resized(image, 4, 4, Resampling::nearest)};
    EXPECT_EQ(value(bigger, 0, 0), 0);
    EXPECT_EQ(value(bigger, 1, 1), 0);
    EXPECT_EQ(value(bigger, 2, 0), 1);
    EXPECT_EQ(value(bigger, 3, 3), 3);
}

TEST(Transform, SupersamplingAveragesWhatEachPixelCovers)
{
    Image checker{PixelFormat::gray_alpha16, 4, 4};
    for(int y{}; y < 4; ++ y)
    {
        for(int x{}; x < 4; ++ x)
        {
            checker.set_pixel(x, y, GrayAlpha16{{static_cast<std::uint16_t>((x + y) % 2 == 0 ? 65535 : 0), 65535}});
        }
    }
    const auto smaller{resized(checker, 2, 2, Resampling::supersampling)};
    EXPECT_NEAR(smaller.pixel<GrayAlpha16>(0, 0).value[0], 32768, 1);
}

TEST(Transform, SeeThroughPixelsLeaveNoDarkFringeWhenMixed)
{
    // Red beside clear black: mixed, the colour stays red, only less solid.
    Image image{PixelFormat::rgba8, 2, 1};
    image.set_pixel(0, 0, Rgba8{{255, 0, 0, 255}});
    image.set_pixel(1, 0, Rgba8{{0, 0, 0, 0}});
    const auto mixed{transformed(image, Affine::translation(0.5, 0), 1, 1, Resampling::bilinear)};
    EXPECT_EQ(mixed.pixel<Rgba8>(0, 0).value[0], 255);
    EXPECT_NEAR(mixed.pixel<Rgba8>(0, 0).alpha(), 128, 1);
}

TEST(Transform, AnAffineAndItsInverseUndoEachOther)
{
    const auto forward{Affine::translation(10, -3).after(Affine::rotation(0.7)).after(Affine::scale(2, 0.5))};
    const auto back{forward.inverse()};
    double x{};
    double y{};
    forward.apply(3, 4, x, y);
    back.apply(x, y, x, y);
    EXPECT_NEAR(x, 3, 1e-9);
    EXPECT_NEAR(y, 4, 1e-9);
}

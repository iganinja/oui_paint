#include "effects/pieces.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

WorkImage dot(int size)
{
    WorkImage image{size, size};
    for(int y{}; y < size; ++ y)
    {
        for(int x{}; x < size; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            const auto lit{x == size / 2 && y == size / 2};
            pixel[0] = pixel[1] = pixel[2] = lit ? 1.0f : 0.0f;
            pixel[3] = 1.0f;
        }
    }
    return image;
}

} // namespace

TEST(Pieces, AGaussianBlurSpreadsADotAndKeepsItsLight)
{
    const auto blurred{gaussian_blur(dot(21), 4.0)};
    auto total{0.0};
    for(int y{}; y < 21; ++ y)
    {
        for(int x{}; x < 21; ++ x)
        {
            total += blurred.at(x, y)[0];
        }
    }
    EXPECT_NEAR(total, 1.0, 1e-4);
    EXPECT_LT(blurred.at(10, 10)[0], 0.5f);
    EXPECT_GT(blurred.at(12, 10)[0], 0.0f);
    EXPECT_FLOAT_EQ(blurred.at(10, 12)[0], blurred.at(12, 10)[0]) << "the same every way";
}

TEST(Pieces, AMotionBlurSmearsAlongItsAngleOnly)
{
    const auto smeared{motion_blur(dot(21), 0.0, 8.0)};
    EXPECT_GT(smeared.at(13, 10)[0], 0.0f);
    EXPECT_FLOAT_EQ(smeared.at(10, 13)[0], 0.0f);
}

TEST(Pieces, EdgeDetectionFindsTheEdgeAndFlatPlacesStayGrey)
{
    WorkImage half{10, 10};
    for(int y{}; y < 10; ++ y)
    {
        for(int x{}; x < 10; ++ x)
        {
            auto* const pixel{half.at(x, y)};
            pixel[0] = pixel[1] = pixel[2] = x < 5 ? 0.0f : 1.0f;
            pixel[3] = 1.0f;
        }
    }
    const auto edges{convolve(half, {-1, 0, 1, -2, 0, 2, -1, 0, 1}, 3, 1.0, 0.5)};
    EXPECT_NEAR(edges.at(2, 5)[0], 0.5, 1e-6);
    EXPECT_GT(edges.at(5, 5)[0], 0.9f);
}

TEST(Pieces, CloudsFillTheWholeRangeAndRepeatForTheSameSeed)
{
    const auto first{clouds(64, 64, 32.0, 0.5, 7)};
    const auto again{clouds(64, 64, 32.0, 0.5, 7)};
    const auto other{clouds(64, 64, 32.0, 0.5, 8)};
    EXPECT_EQ(first.pixels, again.pixels);
    EXPECT_NE(first.pixels, other.pixels);
    auto lowest{1.0f};
    auto highest{0.0f};
    for(int y{}; y < 64; ++ y)
    {
        for(int x{}; x < 64; ++ x)
        {
            lowest = std::min(lowest, first.at(x, y)[0]);
            highest = std::max(highest, first.at(x, y)[0]);
        }
    }
    EXPECT_FLOAT_EQ(lowest, 0.0f);
    EXPECT_FLOAT_EQ(highest, 1.0f);
}

TEST(Pieces, ATableInvertsAndSixteenBitsComeBackWhole)
{
    Image ramp{PixelFormat::gray_alpha16, 1000, 1};
    for(int x{}; x < 1000; ++ x)
    {
        ramp.set_pixel(x, 0, GrayAlpha16{{static_cast<std::uint16_t>(x * 65), 65535}});
    }
    EXPECT_EQ(from_work(to_work(ramp), PixelFormat::gray_alpha16).bytes, ramp.bytes);
    const std::vector<double> inverse{1.0, 0.0};
    const auto inverted{from_work(lookup(to_work(ramp), {inverse, inverse, inverse}), PixelFormat::gray_alpha16)};
    EXPECT_EQ(inverted.pixel<GrayAlpha16>(0, 0).value[0], 65535);
    EXPECT_EQ(inverted.pixel<GrayAlpha16>(999, 0).value[0], 65535 - 999 * 65);
}

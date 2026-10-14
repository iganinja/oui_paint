#include "effects/pieces.h"

#include <gtest/gtest.h>

#include <cmath>

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

TEST(Pieces, AWideGaussianBlurHasAGaussiansShapeAndKeepsItsLight)
{
    const auto blurred{gaussian_blur(dot(201), 30.0)};
    auto total{0.0};
    for(int y{}; y < 201; ++ y)
    {
        for(int x{}; x < 201; ++ x)
        {
            total += blurred.at(x, y)[0];
        }
    }
    EXPECT_NEAR(total, 1.0, 1e-3);
    // The radius is two sigmas, as the narrow one's.
    const auto middle{blurred.at(100, 100)[0]};
    for(const auto distance : {5, 10, 15, 20, 25})
    {
        const auto expected{std::exp(-distance * distance / (2.0 * 15.0 * 15.0))};
        EXPECT_NEAR(blurred.at(100 + distance, 100)[0] / middle, expected, 0.06) << distance;
        EXPECT_NEAR(blurred.at(100, 100 - distance)[0], blurred.at(100 + distance, 100)[0], 1e-6) << distance;
    }
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

TEST(Pieces, ALuminosityTableChangesHowLightAColourIsAndKeepsItsHue)
{
    WorkImage image{2, 1};
    const float orange[4]{0.8f, 0.4f, 0.2f, 1.0f};
    const float half_blue[4]{0.0f, 0.0f, 0.25f, 0.5f};
    std::copy(orange, orange + 4, image.at(0, 0));
    std::copy(half_blue, half_blue + 4, image.at(1, 0));
    // Everything half as light.
    const auto result{lookup_luminosity(image, {0.0, 0.5})};
    const auto* const darker{result.at(0, 0)};
    EXPECT_NEAR(darker[0], 0.4f, 1e-5);
    EXPECT_NEAR(darker[1], 0.2f, 1e-5);
    EXPECT_NEAR(darker[2], 0.1f, 1e-5);
    EXPECT_FLOAT_EQ(result.at(1, 0)[3], 0.5f) << "alpha stays";
    EXPECT_NEAR(result.at(1, 0)[2], 0.125f, 1e-5);
    // Everything white: a colour past white goes to it.
    const auto white{lookup_luminosity(image, {1.0, 1.0})};
    EXPECT_NEAR(white.at(0, 0)[0], 1.0f, 1e-5);
    EXPECT_NEAR(white.at(0, 0)[2], 1.0f, 1e-5);
}

TEST(Pieces, NoiseIsTheSameForTheSameSeedAndCoversWhatItIsAskedTo)
{
    WorkImage gray{64, 64};
    for(std::size_t at{}; at < gray.pixels.size(); ++ at)
    {
        gray.pixels[at] = at % 4 == 3 ? 1.0f : 0.5f;
    }
    const auto first{add_noise(gray, 0.3, 1.0, 0.5, 7)};
    EXPECT_EQ(first.pixels, add_noise(gray, 0.3, 1.0, 0.5, 7).pixels);
    EXPECT_NE(first.pixels, add_noise(gray, 0.3, 1.0, 0.5, 8).pixels);
    auto changed{0};
    for(int y{}; y < 64; ++ y)
    {
        for(int x{}; x < 64; ++ x)
        {
            const auto* const pixel{first.at(x, y)};
            changed += pixel[0] != 0.5f ? 1 : 0;
            EXPECT_GE(pixel[0], 0.05f - 1e-6f) << "lighter or darker by 0.3, and the channel by half that again";
            EXPECT_LE(pixel[0], 0.95f + 1e-6f);
        }
    }
    EXPECT_NEAR(changed / 4096.0, 0.5, 0.05) << "half the pixels";
    EXPECT_EQ(add_noise(gray, 0.3, 1.0, 0.0, 7).pixels, gray.pixels) << "none covered";
}

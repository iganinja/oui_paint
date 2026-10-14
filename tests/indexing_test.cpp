#include "effects/indexing.h"

#include <gtest/gtest.h>

#include <cmath>
#include <set>

using namespace oui_paint;

namespace
{

// An image of the colours given, row by row, `width` wide; a negative red makes a see-through pixel.
WorkImage made(int width, const std::vector<Color>& colors)
{
    WorkImage image{width, static_cast<int>(colors.size()) / width};
    for(std::size_t index{}; index < colors.size(); ++ index)
    {
        auto* const pixel{image.at(static_cast<int>(index) % width, static_cast<int>(index) / width)};
        const auto& color{colors[index]};
        if(color.red < 0)
        {
            std::fill(pixel, pixel + 4, 0.0f);
            continue;
        }
        pixel[0] = static_cast<float>(color.red);
        pixel[1] = static_cast<float>(color.green);
        pixel[2] = static_cast<float>(color.blue);
        pixel[3] = 1.0f;
    }
    return image;
}

WorkImage gray(int width, int height, float value)
{
    WorkImage image{width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = 1.0f;
        }
    }
    return image;
}

int index_at(const WorkImage& indices, int x, int y)
{
    return static_cast<int>(std::lround(indices.at(x, y)[0] * 255.0f));
}

const Color red{1, 0, 0, 1};
const Color green{0, 1, 0, 1};
const Color blue{0, 0, 1, 1};
const Color hole{-1, 0, 0, 0};

} // namespace

TEST(Indexing, TheDistinctColoursAreCountedTheCommonestFirstButNotWhatIsSeeThrough)
{
    const auto image{made(3, {red, green, green, blue, green, hole})};
    const auto found{distinct_colors(image, 10)};
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found[0].first, green);
    EXPECT_EQ(found[0].second, 3u);
    EXPECT_EQ(distinct_colors(image, 2).size(), 2u) << "no more than asked";
}

TEST(Indexing, MedianCutFindsTheColoursThereAreWhenThereAreFewEnough)
{
    const auto image{made(4, {red, red, green, blue, red, green, blue, blue})};
    const auto palette{median_cut(image, 8)};
    EXPECT_EQ(std::set<std::uint32_t>({0xff0000, 0x00ff00, 0x0000ff}).size(), palette.size())
        << "three colours, three entries, however many were asked";
    const auto two{median_cut(gray(2, 2, 0.5f), 4)};
    EXPECT_EQ(two.size(), 1u) << "one colour, one entry";
    EXPECT_NEAR(two[0].red, 128 / 255.0, 1e-9);
    const auto halves{median_cut(made(2, {Color{0, 0, 0, 1}, Color{0.1, 0.1, 0.1, 1}, Color{0.9, 0.9, 0.9, 1},
                                          Color{1, 1, 1, 1}}),
                                 2)};
    ASSERT_EQ(halves.size(), 2u);
    EXPECT_LT(std::min(halves[0].red, halves[1].red), 0.1) << "the dark half averaged";
    EXPECT_GT(std::max(halves[0].red, halves[1].red), 0.9) << "and the light one";
}

TEST(Indexing, EachPixelTakesTheNearestColoursIndexAndSeeThroughStaysSo)
{
    const std::vector<Color> palette{red, green, blue};
    const auto image{made(4, {blue, Color{0.9, 0.1, 0, 1}, green, hole})};
    const auto indices{nearest_indices(image, palette)};
    EXPECT_EQ(index_at(indices, 0, 0), 2);
    EXPECT_EQ(index_at(indices, 1, 0), 0) << "nearly red: red";
    EXPECT_EQ(index_at(indices, 2, 0), 1);
    EXPECT_EQ(indices.at(3, 0)[3], 0.0f) << "see-through";
    EXPECT_EQ(indices.at(0, 0)[3], 1.0f);
}

TEST(Indexing, DitheringMakesAGreyOfBlackAndWhite)
{
    const std::vector<Color> palette{Color{0, 0, 0, 1}, Color{1, 1, 1, 1}};
    const auto image{gray(16, 16, 0.5f)};
    for(const auto& indices : {floyd_steinberg_indices(image, palette, 1.0), bayer_indices(image, palette, 4, 1.0)})
    {
        auto white{0};
        for(int y{}; y < 16; ++ y)
        {
            for(int x{}; x < 16; ++ x)
            {
                white += index_at(indices, x, y);
            }
        }
        EXPECT_NEAR(white, 128, 16) << "about half of them white";
    }
    const auto none{floyd_steinberg_indices(image, palette, 0.0)};
    const auto nearest{nearest_indices(image, palette)};
    EXPECT_EQ(none.pixels, nearest.pixels) << "without strength, the nearest colour each";
}

TEST(Indexing, WithoutStrengthDitheringPicksTheNearestColourAsThePlainWayDoes)
{
    // Colours of 8 bits a channel, so the two see the same colour: a dither looks up what it has worked out before.
    std::uint32_t state{12345u};
    const auto next{[&state]
                    {
                        state ^= state << 13u;
                        state ^= state >> 17u;
                        state ^= state << 5u;
                        return static_cast<float>(state % 256u) / 255.0f;
                    }};
    std::vector<Color> palette;
    for(int index{}; index < 200; ++ index)
    {
        palette.push_back({next(), next(), next(), 1.0});
    }
    WorkImage image{97, 61};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            pixel[0] = next();
            pixel[1] = next();
            pixel[2] = next();
            pixel[3] = 1.0f;
        }
    }
    EXPECT_EQ(floyd_steinberg_indices(image, palette, 0.0).pixels, nearest_indices(image, palette).pixels);
}

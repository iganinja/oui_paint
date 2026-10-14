#include "tools/text.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>

using namespace oui_paint;

namespace
{

// The rows of a layer that hold any ink.
std::pair<int, int> inked_rows(const Layer& layer, int width, int height)
{
    const auto image{layer.read(0, 0, width, height)};
    auto first{-1};
    auto last{-1};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            if(image.pixel<Rgba8>(x, y).alpha() != 0)
            {
                first = first < 0 ? y : first;
                last = y;
                break;
            }
        }
    }
    return {first, last};
}

} // namespace

TEST(Text, TextIsDrawnFromItsTopLeftAndANewLineGoesDown)
{
    auto font{olagarro::oui::built_in_interface_font(24.0f)};
    const Selection everything{200, 100};
    Layer one{PixelFormat::rgba8, "One"};
    draw_text(one, everything, *font, "Hi", {10, 10}, {0, 0, 0, 1}, true);
    const auto [first, last]{inked_rows(one, 200, 100)};
    EXPECT_GE(first, 10);
    EXPECT_LT(last, 10 + font->line_height());
    Layer two{PixelFormat::rgba8, "Two"};
    draw_text(two, everything, *font, "Hi\nHi", {10, 10}, {0, 0, 0, 1}, true);
    EXPECT_GE(inked_rows(two, 200, 100).second, 10 + font->line_height()) << "the second line is under the first";
}

TEST(Text, WithoutAntialiasingEveryInkedPixelIsSolid)
{
    auto font{olagarro::oui::built_in_interface_font(24.0f)};
    const Selection everything{100, 50};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_text(layer, everything, *font, "A", {5, 5}, {0, 0, 0, 1}, false);
    const auto image{layer.read(0, 0, 100, 50)};
    for(int y{}; y < 50; ++ y)
    {
        for(int x{}; x < 100; ++ x)
        {
            const auto alpha{image.pixel<Rgba8>(x, y).alpha()};
            EXPECT_TRUE(alpha == 0 || alpha == 255) << x << ", " << y;
        }
    }
}

namespace
{

// The first and last columns and rows of a layer holding any ink: left, top, right, bottom.
std::array<int, 4> inked_box(const Layer& layer, int width, int height)
{
    const auto image{layer.read(0, 0, width, height)};
    std::array<int, 4> box{width, height, -1, -1};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            if(image.pixel<Rgba8>(x, y).alpha() != 0)
            {
                box = {std::min(box[0], x), std::min(box[1], y), std::max(box[2], x), std::max(box[3], y)};
            }
        }
    }
    return box;
}

constexpr int side{160};

// "I" without antialiasing, with these effects around it, in black, its outline and glow in white.
Layer letter(TextEffects effects)
{
    auto font{olagarro::oui::built_in_interface_font(32.0f)};
    effects.color = {1, 1, 1, 1};
    Layer layer{PixelFormat::rgba8, "Layer"};
    draw_text(layer, Selection{side, side}, *font, "I", {60, 60}, {0, 0, 0, 1}, false, effects);
    return layer;
}

} // namespace

TEST(Text, AnOutlineGoesItsWidthAroundTheTextInTheEffectsColorAndLeavesTheTextAsItIs)
{
    const auto plain{letter({})};
    TextEffects effects;
    effects.outline = 3;
    const auto outlined{letter(effects)};
    const auto box{inked_box(plain, side, side)};
    EXPECT_EQ(inked_box(outlined, side, side), (std::array<int, 4>{box[0] - 3, box[1] - 3, box[2] + 3, box[3] + 3}));
    const auto before{plain.read(0, 0, side, side)};
    const auto after{outlined.read(0, 0, side, side)};
    auto ringed{0};
    for(int y{}; y < side; ++ y)
    {
        for(int x{}; x < side; ++ x)
        {
            const auto was{before.pixel<Rgba8>(x, y)};
            const auto is{after.pixel<Rgba8>(x, y)};
            if(was.alpha() != 0)
            {
                EXPECT_EQ(is.value, was.value) << "the text at " << x << ", " << y;
            }
            else if(is.alpha() != 0)
            {
                EXPECT_EQ(is.value, (Rgba8{{255, 255, 255, 255}}.value)) << "the outline at " << x << ", " << y;
                ++ ringed;
            }
        }
    }
    EXPECT_GT(ringed, 0);
}

TEST(Text, AShadowFallsDownAndToTheRightOnly)
{
    const auto plain{inked_box(letter({}), side, side)};
    TextEffects effects;
    effects.shadow = 4;
    const auto shadowed{inked_box(letter(effects), side, side)};
    EXPECT_EQ(shadowed[0], plain[0]);
    EXPECT_EQ(shadowed[1], plain[1]);
    EXPECT_GE(shadowed[2], plain[2] + 4);
    EXPECT_GE(shadowed[3], plain[3] + 4);
}

TEST(Text, AGlowFadesAsItGoesOutAndEndsAtItsReach)
{
    auto font{olagarro::oui::built_in_interface_font(32.0f)};
    TextEffects effects;
    effects.glow = 10;
    effects.color = {1, 1, 1, 1};
    Layer plain{PixelFormat::rgba8, "Plain"};
    draw_text(plain, Selection{side, side}, *font, "I", {60, 60}, {0, 0, 0, 1}, true);
    Layer glowing{PixelFormat::rgba8, "Glowing"};
    draw_text(glowing, Selection{side, side}, *font, "I", {60, 60}, {0, 0, 0, 1}, true, effects);
    const auto box{inked_box(plain, side, side)};
    const auto image{glowing.read(0, 0, side, side)};
    const auto middle{(box[1] + box[3]) / 2};
    const auto alpha{[&](int x) { return image.pixel<Rgba8>(x, middle).alpha(); }};
    EXPECT_GT(alpha(box[2] + 2), alpha(box[2] + 6));
    EXPECT_GT(alpha(box[2] + 6), 0);
    EXPECT_EQ(alpha(box[2] + 12), 0);
}

TEST(Text, WithoutAntialiasingWhatGoesAroundTheTextIsSolidToo)
{
    TextEffects effects;
    effects.outline = 2;
    effects.shadow = 3;
    effects.glow = 6;
    const auto image{letter(effects).read(0, 0, side, side)};
    for(int y{}; y < side; ++ y)
    {
        for(int x{}; x < side; ++ x)
        {
            const auto alpha{image.pixel<Rgba8>(x, y).alpha()};
            EXPECT_TRUE(alpha == 0 || alpha == 255) << x << ", " << y;
        }
    }
}

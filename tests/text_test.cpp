#include "tools/text.h"

#include <gtest/gtest.h>

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

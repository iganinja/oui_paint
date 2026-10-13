#include "render/display_cache.h"
#include "render/viewport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

using namespace oui_paint;

TEST(Viewport, ASmallImageSitsInTheMiddle)
{
    Viewport viewport;
    viewport.set_view_size(100, 80);
    viewport.set_image_size(20, 10);
    EXPECT_DOUBLE_EQ(viewport.origin().x, 40);
    EXPECT_DOUBLE_EQ(viewport.origin().y, 35);
}

TEST(Viewport, ZoomingKeepsThePointUnderThePointerWhereItIs)
{
    Viewport viewport;
    viewport.set_view_size(100, 100);
    viewport.set_image_size(1000, 1000);
    viewport.scroll_by(-300, -200);
    const Position pointer{30, 70};
    const auto under{viewport.image_at(pointer)};
    viewport.zoom_in(pointer);
    EXPECT_DOUBLE_EQ(viewport.zoom(), 1.5);
    EXPECT_NEAR(viewport.image_at(pointer).x, under.x, 1e-9);
    EXPECT_NEAR(viewport.image_at(pointer).y, under.y, 1e-9);
    viewport.zoom_out(pointer);
    viewport.zoom_out(pointer);
    EXPECT_DOUBLE_EQ(viewport.zoom(), 0.66);
}

TEST(Viewport, ABigImageScrollsNoFurtherThanItsEdges)
{
    Viewport viewport;
    viewport.set_view_size(100, 100);
    viewport.set_image_size(300, 300);
    viewport.scroll_by(50, 50);
    EXPECT_DOUBLE_EQ(viewport.origin().x, 0);
    viewport.scroll_by(-1000, -1000);
    EXPECT_DOUBLE_EQ(viewport.origin().x, -200);
    EXPECT_DOUBLE_EQ(viewport.origin().y, -200);
}

TEST(Viewport, FittingTakesTheBiggestStepUpToActualSize)
{
    Viewport viewport;
    viewport.set_view_size(800, 600);
    viewport.set_image_size(4000, 1000);
    viewport.zoom_to_fit();
    EXPECT_DOUBLE_EQ(viewport.zoom(), 0.16);
    viewport.set_image_size(100, 100);
    viewport.zoom_to_fit();
    EXPECT_DOUBLE_EQ(viewport.zoom(), 1.0);
}

namespace
{

Document two_by_two()
{
    Image image{PixelFormat::rgba8, 2, 2};
    image.set_pixel(0, 0, Rgba8{{255, 0, 0, 255}});
    image.set_pixel(1, 0, Rgba8{{0, 255, 0, 255}});
    image.set_pixel(0, 1, Rgba8{{0, 0, 255, 255}});
    image.set_pixel(1, 1, Rgba8{{0, 0, 0, 0}});
    return Document::from_image(image);
}

} // namespace

TEST(Render, EnlargedPixelsAreSharpAndSeeThroughOnesShowTheCheckerboard)
{
    const auto document{two_by_two()};
    DisplayCache cache{document};
    Viewport viewport;
    viewport.set_view_size(40, 40);
    viewport.set_image_size(2, 2);
    viewport.set_zoom(16.0, {20, 20});
    ASSERT_DOUBLE_EQ(viewport.origin().x, 4);
    std::vector<std::uint32_t> pixels(40 * 40);
    render_view(viewport, cache, 2, 2, pixels.data(), 40, 40, 0x123456u);
    EXPECT_EQ(pixels[0], 0x123456u) << "around the image";
    EXPECT_EQ(pixels[4 * 40 + 4], 0xff0000u);
    EXPECT_EQ(pixels[19 * 40 + 19], 0xff0000u);
    EXPECT_EQ(pixels[19 * 40 + 20], 0x00ff00u);
    EXPECT_EQ(pixels[20 * 40 + 19], 0x0000ffu);
    EXPECT_TRUE(pixels[30 * 40 + 30] == 0xffffffu || pixels[30 * 40 + 30] == 0xccccccu);
}

TEST(Render, AReducedImageShowsTheAverageOfThePixelsEachStandsFor)
{
    Image image{PixelFormat::gray_alpha16, 4, 4};
    for(int y{}; y < 4; ++ y)
    {
        for(int x{}; x < 4; ++ x)
        {
            image.set_pixel(x, y, GrayAlpha16{{static_cast<std::uint16_t>((x + y) % 2 == 0 ? 65535 : 0), 65535}});
        }
    }
    const auto document{Document::from_image(image)};
    DisplayCache cache{document};
    Viewport viewport;
    viewport.set_view_size(2, 2);
    viewport.set_image_size(4, 4);
    viewport.set_zoom(0.5, {0, 0});
    std::vector<std::uint32_t> pixels(4);
    render_view(viewport, cache, 4, 4, pixels.data(), 2, 2, 0);
    for(const auto pixel : pixels)
    {
        EXPECT_NEAR(static_cast<int>(pixel & 0xffu), 128, 1) << "grey, not black or white";
    }
}

TEST(Render, TheSelectionsEdgeIsWhereSelectedMeetsNotSelected)
{
    Selection selection{20, 20};
    selection.add_rectangle({5, 5, 4, 3}, SelectionMode::replace);
    Viewport viewport;
    viewport.set_view_size(20, 20);
    viewport.set_image_size(20, 20);
    ASSERT_DOUBLE_EQ(viewport.origin().x, 0);
    const auto edge{selection_edge(viewport, selection, 20, 20)};
    const auto on_edge{[&](int x, int y) { return std::find(edge.begin(), edge.end(), y * 20 + x) != edge.end(); }};
    EXPECT_TRUE(on_edge(4, 6)) << "left of the selection";
    EXPECT_TRUE(on_edge(8, 6)) << "its last column";
    EXPECT_TRUE(on_edge(6, 4)) << "above it";
    EXPECT_TRUE(on_edge(6, 7)) << "its last row";
    EXPECT_FALSE(on_edge(6, 6)) << "inside";
    EXPECT_FALSE(on_edge(15, 15)) << "far outside";
    std::vector<std::uint32_t> pixels(20 * 20, 0x123456u);
    draw_marching_ants(edge, pixels.data(), 20, 0);
    EXPECT_NE(pixels[6 * 20 + 4], 0x123456u);
    EXPECT_EQ(pixels[6 * 20 + 6], 0x123456u);
    EXPECT_TRUE(selection_edge(viewport, Selection{20, 20}, 20, 20).empty()) << "nothing selected, no edge";
}

TEST(Render, LayersMixForTheScreenByOpacityAndBlendMode)
{
    Document document{4, 1, PixelFormat::rgba8};
    Image blue{PixelFormat::rgba8, 4, 1};
    Image red{PixelFormat::rgba8, 4, 1};
    for(int x{}; x < 4; ++ x)
    {
        blue.set_pixel(x, 0, Rgba8{{0, 0, 255, 255}});
        red.set_pixel(x, 0, Rgba8{{255, 0, 0, x == 3 ? std::uint8_t{128} : std::uint8_t{255}}});
    }
    blue.set_pixel(2, 0, Rgba8{{0, 0, 0, 0}});
    document.add_layer("Blue").write(blue, 0, 0);
    document.add_layer("Red").write(red, 0, 0);
    {
        DisplayCache cache{document};
        const auto& tile{cache.tile({0, 0}, 0)};
        EXPECT_EQ(tile[0], 0xffff0000u) << "opaque over opaque covers it";
        EXPECT_EQ(tile[2], 0xffff0000u) << "over nothing it is all there is";
        EXPECT_NEAR(static_cast<int>(tile[3] >> 16u & 0xffu), 128, 1) << "half see-through mixes";
        EXPECT_NEAR(static_cast<int>(tile[3] & 0xffu), 127, 1);
    }
    document.layers().back().set_opacity(0.5);
    {
        DisplayCache cache{document};
        EXPECT_NEAR(static_cast<int>(cache.tile({0, 0}, 0)[0] & 0xffu), 127, 1) << "half opacity mixes too";
    }
    document.layers().back().set_blend_mode(BlendMode::multiply);
    document.layers().back().set_opacity(1.0);
    {
        DisplayCache cache{document};
        EXPECT_EQ(cache.tile({0, 0}, 0)[0], 0xff000000u) << "red times blue is black";
        EXPECT_EQ(cache.tile({0, 0}, 0)[2], 0xffff0000u) << "multiplied over nothing it shows itself";
    }
}

TEST(Viewport, ZoomingToAnAreaShowsItAllAsBigAsItFitsInTheMiddle)
{
    Viewport viewport;
    viewport.set_view_size(400, 300);
    viewport.set_image_size(2000, 1000);
    viewport.zoom_to(1000, 500, 100, 50);
    EXPECT_DOUBLE_EQ(viewport.zoom(), 4.0) << "the biggest step 100 x 50 fits in 400 x 300";
    const auto middle{viewport.image_at({200, 150})};
    EXPECT_NEAR(middle.x, 1050, 0.5);
    EXPECT_NEAR(middle.y, 525, 0.5);
    viewport.zoom_to(0, 0, 1900, 900);
    EXPECT_LT(viewport.zoom(), 0.25);
    EXPECT_LE(viewport.image_at({0, 0}).x, 0.0) << "the whole area shows";
    EXPECT_GE(viewport.image_at({400, 300}).x, 1900.0);
    EXPECT_GE(viewport.image_at({400, 300}).y, 900.0);
}

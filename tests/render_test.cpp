#include "render/display_cache.h"
#include "render/viewport.h"

#include <gtest/gtest.h>

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

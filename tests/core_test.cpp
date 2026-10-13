#include "core/blend.h"
#include "core/document.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(Image, EightBitsGoToSixteenAndBackUnchanged)
{
    Image image{PixelFormat::gray_alpha8, 256, 1};
    for(int x{}; x < 256; ++ x)
    {
        image.set_pixel(x, 0, GrayAlpha8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(255 - x)}});
    }
    const auto wide{converted(image, PixelFormat::gray_alpha16)};
    EXPECT_EQ(wide.pixel<GrayAlpha16>(0xab, 0).value[0], 0xabab);
    EXPECT_EQ(converted(wide, PixelFormat::gray_alpha8).bytes, image.bytes);
}

TEST(Image, GreyBecomesColourAndColourGreyByItsLuma)
{
    Image gray{PixelFormat::gray_alpha8, 1, 1};
    gray.set_pixel(0, 0, GrayAlpha8{{100, 255}});
    const auto color{converted(gray, PixelFormat::rgba8).pixel<Rgba8>(0, 0)};
    EXPECT_EQ(color.value, (std::array<std::uint8_t, 4>{100, 100, 100, 255}));
    Image red{PixelFormat::rgba8, 1, 1};
    red.set_pixel(0, 0, Rgba8{{255, 0, 0, 255}});
    EXPECT_EQ(converted(red, PixelFormat::gray_alpha8).pixel<GrayAlpha8>(0, 0).value[0], 76);
}

TEST(Layer, WhatIsWrittenAcrossTilesReadsBackAndTheRestIsSeeThrough)
{
    Layer layer{PixelFormat::rgba8, "Layer"};
    Image square{PixelFormat::rgba8, 300, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 300; ++ x)
        {
            square.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 7, 255}});
        }
    }
    layer.write(square, 200, 250);
    EXPECT_EQ(layer.tiles().size(), 4u) << "it reaches two rows of two tiles";
    EXPECT_EQ(layer.read(200, 250, 300, 20).bytes, square.bytes);
    const auto around{layer.read(190, 245, 10, 5)};
    for(const auto byte : around.bytes)
    {
        EXPECT_EQ(byte, 0);
    }
}

TEST(Layer, ACopySharesItsTilesUntilOneIsWrittenTo)
{
    Layer layer{PixelFormat::gray_alpha16, "Layer"};
    Image dot{PixelFormat::gray_alpha16, 1, 1};
    dot.set_pixel(0, 0, GrayAlpha16{{1000, 65535}});
    layer.write(dot, 5, 5);
    const auto kept{layer};
    EXPECT_EQ(kept.tile({0, 0}), layer.tile({0, 0})) << "copying costs nothing";
    dot.set_pixel(0, 0, GrayAlpha16{{2000, 65535}});
    layer.write(dot, 5, 5);
    EXPECT_NE(kept.tile({0, 0}), layer.tile({0, 0}));
    EXPECT_EQ(kept.read(5, 5, 1, 1).pixel<GrayAlpha16>(0, 0).value[0], 1000);
    EXPECT_EQ(layer.read(5, 5, 1, 1).pixel<GrayAlpha16>(0, 0).value[0], 2000);
}

TEST(Document, VisibleLayersAreMixedFromTheBottomUp)
{
    Document document{2, 1, PixelFormat::rgba8};
    Image white{PixelFormat::rgba8, 2, 1};
    white.set_pixel(0, 0, Rgba8{{255, 255, 255, 255}});
    white.set_pixel(1, 0, Rgba8{{255, 255, 255, 255}});
    document.add_layer("Background").write(white, 0, 0);
    Image black{PixelFormat::rgba8, 1, 1};
    black.set_pixel(0, 0, Rgba8{{0, 0, 0, 255}});
    auto& top{document.add_layer("Top")};
    top.write(black, 0, 0);
    top.set_opacity(0.5);
    auto& hidden{document.add_layer("Hidden")};
    hidden.write(black, 1, 0);
    hidden.set_visible(false);
    const auto flat{document.flatten()};
    EXPECT_EQ(flat.pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{128, 128, 128, 255}));
    EXPECT_EQ(flat.pixel<Rgba8>(1, 0).value, (std::array<std::uint8_t, 4>{255, 255, 255, 255}));
}

TEST(Blend, ModesMixSolidPixelsAsPaintNetDoes)
{
    const Rgba8 below{{200, 100, 50, 255}};
    const Rgba8 above{{100, 100, 200, 255}};
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::normal).value, above.value);
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::multiply).value, (std::array<std::uint8_t, 4>{78, 39, 39, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::screen).value, (std::array<std::uint8_t, 4>{222, 161, 211, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::difference).value, (std::array<std::uint8_t, 4>{100, 0, 150, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::lighten).value, (std::array<std::uint8_t, 4>{200, 100, 200, 255}));
    EXPECT_EQ(blend(below, above, 1.0, BlendMode::xor_).value,
              (std::array<std::uint8_t, 4>{200 ^ 100, 0, 50 ^ 200, 255}));
}

TEST(Blend, OverNothingALayerShowsAsItIsWhateverItsMode)
{
    const Rgba8 nothing{};
    const Rgba8 above{{100, 150, 200, 255}};
    for(const auto mode : all_blend_modes())
    {
        EXPECT_EQ(blend(nothing, above, 1.0, mode).value, above.value) << blend_mode_name(mode);
    }
}

TEST(Document, LayersAreAddedAboveTheActiveOneAndMovedRemovedAndDuplicated)
{
    Document document{4, 4, PixelFormat::rgba8};
    document.add_layer("A");
    document.add_layer("B");
    document.set_active_layer(0);
    document.insert_layer_above("C");
    const auto names{[&document]
                     {
                         std::string result;
                         for(const auto& layer : document.layers())
                         {
                             result += layer.name() + " ";
                         }
                         return result;
                     }};
    EXPECT_EQ(names(), "A C B ");
    EXPECT_EQ(document.active_layer(), 1u);
    document.duplicate_layer(1);
    EXPECT_EQ(names(), "A C C copy B ");
    EXPECT_EQ(document.active_layer(), 2u);
    document.move_layer(2, 0);
    EXPECT_EQ(names(), "C copy A C B ");
    EXPECT_EQ(document.active_layer(), 0u);
    document.remove_layer(0);
    EXPECT_EQ(names(), "A C B ");
    EXPECT_EQ(document.active_layer(), 0u);
}

TEST(Document, MergingDownMixesTheLayerAsItShows)
{
    Document document{1, 1, PixelFormat::rgba8};
    Image white{PixelFormat::rgba8, 1, 1};
    white.set_pixel(0, 0, Rgba8{{200, 200, 200, 255}});
    document.add_layer("Bottom").write(white, 0, 0);
    Image dark{PixelFormat::rgba8, 1, 1};
    dark.set_pixel(0, 0, Rgba8{{100, 100, 100, 255}});
    auto& top{document.add_layer("Top")};
    top.write(dark, 0, 0);
    top.set_blend_mode(BlendMode::multiply);
    const auto expected{document.flatten().bytes};
    document.merge_down(1);
    ASSERT_EQ(document.layers().size(), 1u);
    EXPECT_EQ(document.flatten().bytes, expected);
    EXPECT_EQ(document.layers()[0].read(0, 0, 1, 1).pixel<Rgba8>(0, 0).value[0], 78);
}

TEST(Document, CroppingToAnEllipseLeavesItsCornersSeeThrough)
{
    Document document{100, 100, PixelFormat::rgba8};
    Image solid{PixelFormat::rgba8, 100, 100};
    for(int y{}; y < 100; ++ y)
    {
        for(int x{}; x < 100; ++ x)
        {
            solid.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y), 0, 255}});
        }
    }
    document.add_layer("Background").write(solid, 0, 0);
    document.selection().add_ellipse(20, 30, 60, 50, SelectionMode::replace);
    document.crop_to_selection();
    EXPECT_EQ(document.width(), 40);
    EXPECT_EQ(document.height(), 20);
    EXPECT_FALSE(document.selection().active());
    const auto flat{document.flatten()};
    EXPECT_EQ(flat.pixel<Rgba8>(0, 0).alpha(), 0);
    EXPECT_EQ(flat.pixel<Rgba8>(20, 10).value, (std::array<std::uint8_t, 4>{40, 40, 0, 255}));
}

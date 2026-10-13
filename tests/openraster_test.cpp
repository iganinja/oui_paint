#include "codecs/openraster.h"
#include "codecs/zip_api.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(OpenRaster, ADocumentComesBackWithItsLayersAsTheyWere)
{
    Document document{300, 200, PixelFormat::gray_alpha16};
    auto& bottom{document.add_layer("Ground & <rocks>")};
    Image ramp{PixelFormat::gray_alpha16, 300, 200};
    for(int y{}; y < 200; ++ y)
    {
        for(int x{}; x < 300; ++ x)
        {
            ramp.set_pixel(x, y, GrayAlpha16{{static_cast<std::uint16_t>(y * 300 + x), 65535}});
        }
    }
    bottom.write(ramp, 0, 0);
    auto& top{document.add_layer("Hills")};
    Image hill{PixelFormat::gray_alpha16, 10, 10};
    hill.set_pixel(3, 3, GrayAlpha16{{40000, 30000}});
    top.write(hill, 100, 50);
    top.set_opacity(0.5);
    top.set_blend_mode(BlendMode::glow);
    top.set_visible(false);

    const auto file{write_openraster(document)};
    EXPECT_EQ(file.substr(30, 8), "mimetype") << "the first entry, as OpenRaster asks";
    EXPECT_EQ(file.substr(38, 16), "image/openraster") << "and stored, not compressed";
    ASSERT_TRUE(zip_api::unzip(file, "mergedimage.png").has_value());
    ASSERT_TRUE(zip_api::unzip(file, "Thumbnails/thumbnail.png").has_value());

    std::string error;
    const auto back{read_openraster(file, error)};
    ASSERT_TRUE(back.has_value()) << error;
    EXPECT_EQ(back->format(), PixelFormat::gray_alpha16);
    ASSERT_EQ(back->layers().size(), 2u);
    EXPECT_EQ(back->layers()[0].name(), "Ground & <rocks>");
    EXPECT_EQ(back->layers()[0].read(0, 0, 300, 200).bytes, ramp.bytes);
    const auto& hills{back->layers()[1]};
    EXPECT_EQ(hills.name(), "Hills");
    EXPECT_DOUBLE_EQ(hills.opacity(), 0.5);
    EXPECT_EQ(hills.blend_mode(), BlendMode::glow);
    EXPECT_FALSE(hills.visible());
    EXPECT_EQ(hills.read(103, 53, 1, 1).pixel<GrayAlpha16>(0, 0).value,
              (std::array<std::uint16_t, 2>{40000, 30000}));
}

TEST(OpenRaster, WhatIsNotOpenRasterSaysSo)
{
    std::string error;
    EXPECT_FALSE(read_openraster("not a zip", error).has_value());
    EXPECT_FALSE(error.empty());
}

TEST(OpenRaster, AnIndexedDocumentComesBackWithItsPalettesAndSlots)
{
    Document document{2, 1, PixelFormat::gray_alpha8};
    auto setup{PaletteSetup::greys(2, 8)};
    setup.palettes[1].colors[3] = {1, 0, 0, 1};
    document.set_palettes(setup);
    document.add_layer("Bottom");
    auto& top{document.add_layer("Top")};
    top.set_palette_slot(1);
    Image dot{PixelFormat::gray_alpha8, 1, 1};
    dot.set_pixel(0, 0, GrayAlpha8{{3, 255}});
    top.write(dot, 1, 0);
    std::string error;
    const auto back{read_openraster(write_openraster(document), error)};
    ASSERT_TRUE(back.has_value()) << error;
    ASSERT_TRUE(back->indexed());
    EXPECT_EQ(back->palettes()->count(), 2);
    EXPECT_EQ(back->layers()[1].palette_slot(), 1);
    EXPECT_EQ(back->layers()[1].read(1, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 3);
    EXPECT_EQ(back->flatten().pixel<Rgba8>(1, 0).value[0], 255);
}

#include "codecs/palettes.h"
#include "core/json.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(Palettes, JsonIsReadWithItsNestingAndEscapes)
{
    const auto json{parse_json(R"({"a": [1, 2.5, true, null], "b": {"c": "x\"yé"}})")};
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ((*json)["a"].items.size(), 4u);
    EXPECT_DOUBLE_EQ((*json)["a"].items[1].number, 2.5);
    EXPECT_TRUE((*json)["a"].items[2].boolean);
    EXPECT_EQ((*json)["b"]["c"].text, "x\"y\xc3\xa9");
    EXPECT_FALSE(parse_json("{\"a\": }").has_value());
}

TEST(Palettes, ASetupComesBackFromItsJson)
{
    auto setup{PaletteSetup::greys(3, 16)};
    setup.palettes[1].name = "Hero \"red\"";
    setup.palettes[1].colors[5] = {1, 0, 0, 1};
    setup.palettes[2].transparent.reset();
    setup.allowed = {{0, 0, 0, 1}, {1, 1, 1, 1}};
    std::string error;
    const auto back{read_palette_setup(write_palette_setup(setup), error)};
    ASSERT_TRUE(back.has_value()) << error;
    EXPECT_EQ(back->size, 16);
    ASSERT_EQ(back->count(), 3);
    EXPECT_EQ(back->palettes[1].name, "Hero \"red\"");
    EXPECT_FALSE(back->palettes[2].transparent.has_value());
    EXPECT_EQ(back->allowed.size(), 2u);
    // Colours go through 8-bit hexadecimal.
    EXPECT_EQ(to_hex(back->palettes[1].colors[5]), "#ff0000");
}

TEST(Palettes, PaletteFilesComeBack)
{
    const std::vector<Color> colors{{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    for(const auto* const name : {"x.pal", "x.gpl", "x.act"})
    {
        std::string error;
        const auto back{read_palette_file(name, write_palette_file(name, colors), error)};
        ASSERT_TRUE(back.has_value()) << name << ": " << error;
        ASSERT_EQ(back->size(), 3u) << name;
        EXPECT_EQ(to_hex((*back)[1]), "#00ff00") << name;
    }
}

TEST(Palettes, AnIndexedPngHoldsEveryPaletteInOrderAndComesBackWithItsPalettesAndLayers)
{
    Document document{3, 1, PixelFormat::gray_alpha8};
    auto setup{PaletteSetup::greys(4, 16)};
    setup.palettes[2].colors[7] = {0, 0.5, 1, 1};
    document.set_palettes(setup);
    auto& bottom{document.add_layer("Bottom")};
    Image indices{PixelFormat::gray_alpha8, 3, 1};
    indices.set_pixel(0, 0, GrayAlpha8{{9, 255}});
    indices.set_pixel(1, 0, GrayAlpha8{{0, 0}});
    indices.set_pixel(2, 0, GrayAlpha8{{4, 255}});
    bottom.write(indices, 0, 0);
    auto& top{document.add_layer("Top")};
    top.set_palette_slot(2);
    Image one{PixelFormat::gray_alpha8, 1, 1};
    one.set_pixel(0, 0, GrayAlpha8{{7, 255}});
    top.write(one, 2, 0);

    const auto file{write_indexed_png(document)};
    EXPECT_TRUE(is_paletted_png(file));
    std::string error;
    const auto back{read_indexed_png(file, error)};
    ASSERT_TRUE(back.has_value()) << error;
    ASSERT_EQ(back->palettes()->count(), 4) << "the palettes as they were, kept beside the PNG's own";
    ASSERT_EQ(back->palettes()->size, 16);
    EXPECT_EQ(to_hex(back->palettes()->palettes[2].colors[7]), "#0080ff");
    // A layer for each palette the pixels use, which shows as the image did.
    ASSERT_EQ(back->layers().size(), 2u);
    EXPECT_EQ(back->layers()[0].palette_slot(), 0);
    EXPECT_EQ(back->layers()[1].palette_slot(), 2);
    const auto bottom_back{back->layers()[0].read(0, 0, 3, 1)};
    EXPECT_EQ(bottom_back.pixel<GrayAlpha8>(0, 0).value[0], 9);
    EXPECT_EQ(bottom_back.pixel<GrayAlpha8>(1, 0).alpha(), 0);
    EXPECT_EQ(bottom_back.pixel<GrayAlpha8>(2, 0).alpha(), 0) << "palette 2's pixel is on its own layer";
    EXPECT_EQ(back->layers()[1].read(2, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 7);
    const auto before{document.flatten()};
    const auto after{back->flatten()};
    for(int x{}; x < 3; ++ x)
    {
        EXPECT_EQ(after.pixel<Rgba8>(x, 0).value, before.pixel<Rgba8>(x, 0).value) << x;
    }
    std::string colors_error;
    const auto palette{read_palette_file("x.png", file, colors_error)};
    ASSERT_TRUE(palette.has_value());
    EXPECT_EQ(palette->size(), 64u);
}

TEST(Palettes, AnIndexedPngOfAnotherProgramComesBackAsOnePaletteOfAllItsColours)
{
    Document document{2, 1, PixelFormat::gray_alpha8};
    document.set_palettes(PaletteSetup::greys(4, 16));
    Image indices{PixelFormat::gray_alpha8, 2, 1};
    indices.set_pixel(0, 0, GrayAlpha8{{9, 255}});
    indices.set_pixel(1, 0, GrayAlpha8{{3, 255}});
    auto& layer{document.add_layer("Only")};
    layer.write(indices, 0, 0);
    layer.set_palette_slot(1);
    const auto file{write_indexed_png(document)};
    // The file without OUI Paint's text chunks, as another program writes it: chunks are a length, a type, the data
    // and a checksum, after an eight-byte signature.
    std::string plain{file.substr(0, 8)};
    for(std::size_t at{8}; at + 8 <= file.size();)
    {
        const auto length{static_cast<std::size_t>(static_cast<unsigned char>(file[at])) << 24u |
                          static_cast<std::size_t>(static_cast<unsigned char>(file[at + 1])) << 16u |
                          static_cast<std::size_t>(static_cast<unsigned char>(file[at + 2])) << 8u |
                          static_cast<std::size_t>(static_cast<unsigned char>(file[at + 3]))};
        if(file.compare(at + 4, 4, "iTXt") != 0)
        {
            plain += file.substr(at, length + 12);
        }
        at += length + 12;
    }
    ASSERT_LT(plain.size(), file.size()) << "it had a text chunk";
    std::string error;
    const auto back{read_indexed_png(plain, error)};
    ASSERT_TRUE(back.has_value()) << error;
    EXPECT_EQ(back->palettes()->count(), 1);
    EXPECT_EQ(back->palettes()->size, 64);
    EXPECT_EQ(back->layers()[0].read(0, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 16 + 9);
}

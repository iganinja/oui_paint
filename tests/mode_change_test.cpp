#include "app/help.h"
#include "effects/mode_change.h"

#include <gtest/gtest.h>

#include <set>

using namespace oui_paint;

namespace
{

PluginHost& built_in()
{
    static PluginHost host;
    static const auto loaded{[]
                             {
                                 for(const auto& [source, name] : built_in_plugins())
                                 {
                                     host.load(source, name);
                                 }
                                 return true;
                             }()};
    static_cast<void>(loaded);
    return host;
}

const Indexer& indexer(std::string_view name)
{
    for(const auto& found : built_in().indexers())
    {
        if(found.name == name)
        {
            return found;
        }
    }
    throw std::runtime_error{"no indexer " + std::string{name}};
}

const Converter& converter(std::string_view name)
{
    for(const auto& found : built_in().converters())
    {
        if(found.name == name)
        {
            return found;
        }
    }
    throw std::runtime_error{"no converter " + std::string{name}};
}

// Two layers: red and green on the bottom, blue over half of it on top, the rest of the top see-through.
Document two_layers(PixelFormat format)
{
    Document document{4, 2, format};
    Image bottom{PixelFormat::rgba8, 4, 2};
    Image top{PixelFormat::rgba8, 4, 2};
    for(int y{}; y < 2; ++ y)
    {
        for(int x{}; x < 4; ++ x)
        {
            bottom.set_pixel(x, y, x < 2 ? Rgba8{{255, 0, 0, 255}} : Rgba8{{0, 255, 0, 255}});
            top.set_pixel(x, y, y == 0 ? Rgba8{{0, 0, 255, 255}} : Rgba8{{0, 0, 0, 0}});
        }
    }
    document.add_layer("Bottom").write(converted(bottom, format), 0, 0);
    document.add_layer("Top").write(converted(top, format), 0, 0);
    return document;
}

} // namespace

TEST(ModeChange, ExactColorsMakeOnePaletteForEveryLayerWithIndexZeroSeeThrough)
{
    auto document{two_layers(PixelFormat::rgba8)};
    const auto flat_before{document.flatten()};
    index_document(built_in(), indexer("Exact colors"), {}, 256, std::nullopt, document);
    ASSERT_TRUE(document.indexed());
    const auto& palette{document.palettes()->palettes.at(0)};
    EXPECT_EQ(palette.transparent, 0);
    EXPECT_EQ(document.palettes()->size, 16) << "a row of 16 holds the see-through index and three colours";
    const auto flat_after{document.flatten()};
    for(int y{}; y < 2; ++ y)
    {
        for(int x{}; x < 4; ++ x)
        {
            EXPECT_EQ(flat_after.pixel<Rgba8>(x, y).value, flat_before.pixel<Rgba8>(x, y).value) << x << ", " << y;
        }
    }
    EXPECT_EQ(document.layers()[1].read(0, 1, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 0);
}

TEST(ModeChange, AnImageWithNothingSeeThroughHasAll256ColoursAndNoSeeThroughIndex)
{
    Document document{16, 16, PixelFormat::rgba8};
    Image pixels{PixelFormat::rgba8, 16, 16};
    for(int y{}; y < 16; ++ y)
    {
        for(int x{}; x < 16; ++ x)
        {
            pixels.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x * 16), static_cast<std::uint8_t>(y * 16), 7, 255}});
        }
    }
    document.add_layer("Only").write(pixels, 0, 0);
    index_document(built_in(), indexer("Exact colors"), {}, 256, std::nullopt, document);
    EXPECT_EQ(document.palettes()->size, 256);
    EXPECT_FALSE(document.palettes()->palettes.at(0).transparent);
    EXPECT_EQ(document.flatten().pixel<Rgba8>(15, 3).value, (std::array<std::uint8_t, 4>{240, 48, 7, 255}));
}

TEST(ModeChange, ExactColorsRefuseMoreColoursThanAsked)
{
    auto document{two_layers(PixelFormat::rgba8)};
    EXPECT_THROW(index_document(built_in(), indexer("Exact colors"), {}, 2, std::nullopt, document), std::exception);
    EXPECT_FALSE(document.indexed()) << "the document as it was";
}

TEST(ModeChange, MedianCutWithDitheringKeepsToTheCountAsked)
{
    Document document{32, 8, PixelFormat::rgba16};
    Image ramp{PixelFormat::rgba8, 32, 8};
    for(int y{}; y < 8; ++ y)
    {
        for(int x{}; x < 32; ++ x)
        {
            ramp.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x * 8), 0, static_cast<std::uint8_t>(255 - x * 8), 255}});
        }
    }
    document.add_layer("Ramp").write(converted(ramp, PixelFormat::rgba16), 0, 0);
    index_document(built_in(), indexer("Median cut"), {{"dither", 1}, {"strength", 100}}, 4, std::nullopt, document);
    std::set<int> used;
    const auto indices{document.layers()[0].read(0, 0, 32, 8)};
    for(int y{}; y < 8; ++ y)
    {
        for(int x{}; x < 32; ++ x)
        {
            used.insert(indices.pixel<GrayAlpha8>(x, y).value[0]);
        }
    }
    EXPECT_LE(used.size(), 4u);
    EXPECT_GE(used.size(), 3u) << "a ramp dithered uses most of them";
}

TEST(ModeChange, AnImageGoesIntoAPaletteGiven)
{
    auto document{two_layers(PixelFormat::rgba8)};
    const std::vector<Color> given{{1, 0, 0, 1}, {0, 0.9, 0, 1}, {0, 0, 0.8, 1}, {1, 1, 1, 1}};
    index_document(built_in(), indexer("Median cut"), {{"dither", 0}, {"strength", 100}}, 256, given, document);
    const auto& palette{document.palettes()->palettes.at(0)};
    EXPECT_EQ(palette.colors[2], given[1]) << "after the see-through index";
    EXPECT_EQ(document.layers()[0].read(3, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 2);
    EXPECT_EQ(document.layers()[1].read(0, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 3);
}

TEST(ModeChange, AConverterTurnsColourGreyByTheWeightsChosen)
{
    auto document{two_layers(PixelFormat::rgba16)};
    convert_document(built_in(), {{&converter("Luminosity"), {{"weights", 4}}}, {&converter("Rounding"), {{"dither", 0}}}},
                     PixelFormat::gray_alpha8, document);
    EXPECT_EQ(document.format(), PixelFormat::gray_alpha8);
    const auto bottom{document.layers()[0].read(0, 0, 4, 1)};
    EXPECT_EQ(bottom.pixel<GrayAlpha8>(0, 0).value[0], 0) << "red counts nothing with green's weights";
    EXPECT_EQ(bottom.pixel<GrayAlpha8>(3, 0).value[0], 255);
}

TEST(ModeChange, TheConversionsAChangeOfModeGoesThrough)
{
    EXPECT_EQ(conversions_between(PixelFormat::rgba16, false, PixelFormat::gray_alpha8),
              (std::vector<std::string>{"grayscale", "8 bits"}));
    EXPECT_EQ(conversions_between(PixelFormat::gray_alpha16, false, PixelFormat::rgba8),
              (std::vector<std::string>{"8 bits"}));
    EXPECT_EQ(conversions_between(PixelFormat::gray_alpha8, true, PixelFormat::gray_alpha16),
              (std::vector<std::string>{"grayscale"}));
    EXPECT_TRUE(conversions_between(PixelFormat::rgba8, false, PixelFormat::rgba16).empty());
}

TEST(ModeChange, TheIndexerAndConverterThePluginsPageShowsWork)
{
    olagarro::oui::CommandRegistry commands;
    std::string text;
    for(const auto& page : help_pages(commands))
    {
        if(page.file == "plugins.md")
        {
            text = page.markdown;
        }
    }
    PluginHost host;
    for(const auto* const declares : {"paint.indexer(", "paint.converter("})
    {
        const auto at{text.find("```js\n" + std::string{declares})};
        ASSERT_NE(at, std::string::npos) << declares;
        const auto begin{at + 6};
        host.load(text.substr(begin, text.find("```", begin) - begin), "example.js");
    }
    auto document{two_layers(PixelFormat::rgba8)};
    convert_document(host, {{&host.converters().at(0), {}}}, PixelFormat::gray_alpha8, document);
    EXPECT_NEAR(document.layers()[0].read(0, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 127.5, 0.5) << "red's lightness";
    auto indexed{two_layers(PixelFormat::rgba8)};
    index_document(host, host.indexers().at(0), {{"dither", 1}}, 256, std::nullopt, indexed);
    EXPECT_EQ(indexed.palettes()->palettes.at(0).colors.size(), 224u) << "216 and the see-through index, in rows of 16";
}

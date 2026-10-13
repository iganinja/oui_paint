#include "core/document.h"
#include "core/history.h"
#include "render/display_cache.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

// Two palettes of four: greys, and reds; index 0 see-through in both.
Document two_palettes()
{
    Document document{4, 1, PixelFormat::gray_alpha8};
    auto setup{PaletteSetup::greys(2, 4)};
    for(int index{}; index < 4; ++ index)
    {
        setup.palettes[1].colors[static_cast<std::size_t>(index)] = {index / 3.0, 0, 0, 1};
    }
    document.set_palettes(setup);
    auto& bottom{document.add_layer("Bottom")};
    Image indices{PixelFormat::gray_alpha8, 4, 1};
    for(int x{}; x < 4; ++ x)
    {
        indices.set_pixel(x, 0, GrayAlpha8{{static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(x == 0 ? 0 : 255)}});
    }
    bottom.write(indices, 0, 0);
    auto& top{document.add_layer("Top")};
    top.set_palette_slot(1);
    Image one{PixelFormat::gray_alpha8, 1, 1};
    one.set_pixel(0, 0, GrayAlpha8{{3, 255}});
    top.write(one, 3, 0);
    return document;
}

} // namespace

TEST(Indexed, EachLayerShowsInItsOwnPaletteAndTheSeeThroughIndexShowsNothing)
{
    const auto document{two_palettes()};
    const auto flat{document.flatten()};
    ASSERT_EQ(flat.format, PixelFormat::rgba8);
    EXPECT_EQ(flat.pixel<Rgba8>(0, 0).alpha(), 0) << "index 0 is see-through";
    EXPECT_EQ(flat.pixel<Rgba8>(2, 0).value, (std::array<std::uint8_t, 4>{170, 170, 170, 255}));
    EXPECT_EQ(flat.pixel<Rgba8>(3, 0).value, (std::array<std::uint8_t, 4>{255, 0, 0, 255})) << "the reds of slot 1";
}

TEST(Indexed, TheDisplayShowsWhatFlatteningGives)
{
    const auto document{two_palettes()};
    DisplayCache cache{document};
    const auto& tile{cache.tile({0, 0}, 0)};
    EXPECT_EQ(tile[3], 0xffff0000u);
    EXPECT_EQ(tile[1], 0xff555555u);
}

TEST(Indexed, ColoursBecomeTheNearestIndicesAndMergingKeepsThePaletteBelow)
{
    auto document{two_palettes()};
    Image colors{PixelFormat::rgba8, 2, 1};
    colors.set_pixel(0, 0, Rgba8{{100, 0, 0, 255}});
    colors.set_pixel(1, 0, Rgba8{{0, 0, 0, 10}});
    const auto indices{document.indices_of(colors, 1)};
    EXPECT_EQ(indices.pixel<GrayAlpha8>(0, 0).value[0], 1) << "a dark red is nearest the first red";
    EXPECT_EQ(indices.pixel<GrayAlpha8>(1, 0).alpha(), 0) << "mostly see-through goes to the see-through index";
    document.merge_down(1);
    ASSERT_EQ(document.layers().size(), 1u);
    // Pure red, in the greys below, is nearest the darker grey (by how eyes weigh red).
    EXPECT_EQ(document.layers()[0].read(3, 0, 1, 1).pixel<GrayAlpha8>(0, 0).value[0], 1);
}

TEST(Indexed, ChangingAPaletteIsUndone)
{
    auto document{two_palettes()};
    History history;
    history.begin_step(document);
    auto setup{*document.palettes()};
    setup.palettes[0].colors[2] = {0, 0, 1, 1};
    document.set_palettes(setup);
    history.finish_step("Edit Palette", document);
    ASSERT_EQ(history.steps().size(), 1u);
    history.undo(document);
    EXPECT_EQ(document.flatten().pixel<Rgba8>(2, 0).value[2], 170);
}

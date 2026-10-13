#include "core/document.h"
#include "tools/stroke.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

GrayAlpha16 at(const Layer& layer, int x, int y)
{
    return layer.read(x, y, 1, 1).pixel<GrayAlpha16>(0, 0);
}

} // namespace

TEST(Stroke, AHardBrushPaintsItsWidthAlongTheLineAndNothingBeyond)
{
    Layer layer{PixelFormat::gray_alpha16, "Heights"};
    BrushSettings settings;
    settings.width = 10.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    const Selection everything{100, 100};
    Stroke stroke{layer, everything, {1.0, 1.0, 1.0, 1.0}, settings, StrokeMode::paint};
    stroke.line_to({20.5, 50.5});
    stroke.line_to({80.5, 50.5});
    EXPECT_EQ(at(layer, 50, 50).value[0], 65535);
    EXPECT_EQ(at(layer, 50, 54).value[0], 65535);
    EXPECT_EQ(at(layer, 50, 56).alpha(), 0) << "past the radius";
    EXPECT_EQ(at(layer, 14, 50).alpha(), 0) << "before the round end";
    EXPECT_EQ(at(layer, 16, 50).value[0], 65535);
}

TEST(Stroke, ASeeThroughColourDoesNotBuildUpWhereTheStrokeCrossesItself)
{
    Layer layer{PixelFormat::rgba8, "Layer"};
    BrushSettings settings;
    settings.width = 6.0;
    settings.hardness = 1.0;
    const Selection everything{64, 64};
    Stroke stroke{layer, everything, {1.0, 0.0, 0.0, 0.5}, settings, StrokeMode::paint};
    stroke.line_to({10.5, 30.5});
    stroke.line_to({50.5, 30.5});
    stroke.line_to({10.5, 30.5});
    stroke.line_to({30.5, 10.5});
    stroke.line_to({30.5, 50.5});
    EXPECT_EQ(layer.read(30, 30, 1, 1).pixel<Rgba8>(0, 0).alpha(), 128);
}

TEST(Stroke, TheEraserTakesAlphaAwayAndOnlyTheTilesWrittenAreTold)
{
    Document document{600, 100, PixelFormat::gray_alpha8};
    auto& layer{document.add_layer("Layer")};
    Image solid{PixelFormat::gray_alpha8, 600, 100};
    for(int y{}; y < 100; ++ y)
    {
        for(int x{}; x < 600; ++ x)
        {
            solid.set_pixel(x, y, GrayAlpha8{{200, 255}});
        }
    }
    layer.write(solid, 0, 0);
    BrushSettings settings;
    settings.width = 4.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    Stroke stroke{layer, document.selection(), {0, 0, 0, 1}, settings, StrokeMode::erase};
    stroke.line_to({300.5, 20.5});
    stroke.line_to({310.5, 20.5});
    EXPECT_EQ(layer.read(305, 20, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 0);
    EXPECT_EQ(layer.read(305, 30, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 255);
    EXPECT_EQ(stroke.take_changed(), (std::vector<TilePoint>{{1, 0}}));
    EXPECT_TRUE(stroke.take_changed().empty());
}

TEST(Stroke, PaintGoesOnlyWhereTheSelectionSelects)
{
    Layer layer{PixelFormat::gray_alpha8, "Layer"};
    Selection selection{100, 100};
    selection.add_rectangle({0, 0, 50, 100}, SelectionMode::replace);
    BrushSettings settings;
    settings.width = 4.0;
    settings.hardness = 1.0;
    settings.antialias = false;
    Stroke stroke{layer, selection, {1, 1, 1, 1}, settings, StrokeMode::paint};
    stroke.line_to({10.5, 50.5});
    stroke.line_to({90.5, 50.5});
    EXPECT_EQ(layer.read(40, 50, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 255);
    EXPECT_EQ(layer.read(60, 50, 1, 1).pixel<GrayAlpha8>(0, 0).alpha(), 0);
}

TEST(Stroke, LiftedPixelsLaidElsewhereKeepTheirSelectedShape)
{
    Layer layer{PixelFormat::rgba8, "Layer"};
    Image solid{PixelFormat::rgba8, 20, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 20; ++ x)
        {
            solid.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x * 10), 0, 0, 255}});
        }
    }
    layer.write(solid, 0, 0);
    Selection selection{100, 100};
    selection.add_rectangle({2, 2, 3, 3}, SelectionMode::replace);
    const auto lifted{lift_selected(layer, selection)};
    EXPECT_EQ(lifted.x, 2);
    EXPECT_EQ(lifted.pixels.width, 3);
    static_cast<void>(erase_selected(layer, selection));
    static_cast<void>(lay_over(layer, lifted.pixels, 50, 60));
    EXPECT_EQ(layer.read(3, 3, 1, 1).pixel<Rgba8>(0, 0).alpha(), 0) << "a hole where it was";
    EXPECT_EQ(layer.read(51, 61, 1, 1).pixel<Rgba8>(0, 0).value, (std::array<std::uint8_t, 4>{30, 0, 0, 255}));
}

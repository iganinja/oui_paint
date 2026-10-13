#include "tools/fill.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

// A white image with a black ring around (5..14, 5..14), its inside light grey.
Document ringed()
{
    Document document{20, 20, PixelFormat::gray_alpha16};
    Image image{PixelFormat::gray_alpha16, 20, 20};
    for(int y{}; y < 20; ++ y)
    {
        for(int x{}; x < 20; ++ x)
        {
            const auto ring{(x == 5 || x == 14 || y == 5 || y == 14) && x >= 5 && x <= 14 && y >= 5 && y <= 14};
            const auto inside{x > 5 && x < 14 && y > 5 && y < 14};
            const std::uint16_t value{static_cast<std::uint16_t>(ring ? 0 : inside ? 60000 : 65535)};
            image.set_pixel(x, y, GrayAlpha16{{value, 65535}});
        }
    }
    document.add_layer("Background").write(image, 0, 0);
    return document;
}

std::uint16_t at(const Document& document, int x, int y)
{
    return document.flatten().pixel<GrayAlpha16>(x, y).value[0];
}

} // namespace

TEST(Fill, AContiguousFillStopsAtWhatIsTooDifferent)
{
    auto document{ringed()};
    FillSettings settings;
    settings.tolerance = 0.0;
    bucket_fill(document, 0, 0, {0.5, 0.5, 0.5, 1.0}, settings);
    EXPECT_NEAR(at(document, 0, 0), 32768, 1);
    EXPECT_EQ(at(document, 5, 5), 0) << "the ring";
    EXPECT_EQ(at(document, 9, 9), 60000) << "inside the ring, not joined";
}

TEST(Fill, ToleranceTakesSimilarPixelsAndGlobalTakesThemAnywhere)
{
    auto document{ringed()};
    FillSettings settings;
    settings.tolerance = 0.1;
    settings.global = true;
    bucket_fill(document, 0, 0, {0.0, 0.0, 0.0, 1.0}, settings);
    EXPECT_EQ(at(document, 9, 9), 0) << "light grey is within a tenth of white";
    EXPECT_EQ(at(document, 19, 19), 0);
}

TEST(Fill, AFillStaysInsideTheSelection)
{
    auto document{ringed()};
    document.selection().add_rectangle({0, 0, 3, 20}, SelectionMode::replace);
    bucket_fill(document, 0, 0, {0.0, 0.0, 0.0, 1.0}, {});
    EXPECT_EQ(at(document, 1, 10), 0);
    EXPECT_EQ(at(document, 4, 10), 65535);
}

#include "ui/icons.h"

#include <gtest/gtest.h>

using namespace olagarro;

TEST(Icons, GodotsDarkInterfaceColoursTakeTheirLightOnes)
{
    EXPECT_EQ(oui_paint::for_light_interface(R"(<path fill="#e0e0e0"/><path fill="#FFF"/><path fill="#123456"/>)"),
              R"(<path fill="#5a5a5a"/><path fill="#414141"/><path fill="#123456"/>)");
}

TEST(Icons, AnIconIsDrawnAtAWholeMultipleOfItsSizeInTheMiddle)
{
    oui_paint::Icons icons;
    const auto* const picture{icons.picture("Bucket", 24, 26)};
    ASSERT_NE(picture, nullptr);
    EXPECT_EQ(picture->width, 24);
    EXPECT_EQ(picture->height, 26);
    // Drawn 16 pixels a side from (4, 5): nothing outside that square.
    for(int y{}; y < 26; ++ y)
    {
        for(int x{}; x < 24; ++ x)
        {
            if(x < 4 || x >= 20 || y < 5 || y >= 21)
            {
                EXPECT_EQ(picture->pixels[static_cast<std::size_t>(y * 24 + x)] >> 24u, 0u) << x << ", " << y;
            }
        }
    }
    EXPECT_EQ(icons.picture("Bucket", 24, 26), picture) << "drawn once and kept";
    EXPECT_EQ(icons.picture("NoSuchIcon", 24, 24), nullptr);
}

TEST(Icons, OnADarkInterfaceTheyAreDrawnLighterAsGodotDrawsThem)
{
    // The mean brightness of what an icon covers.
    const auto brightness{[](const olagarro::oui::Picture& picture)
                          {
                              double total{};
                              double covered{};
                              for(const auto pixel : picture.pixels)
                              {
                                  const auto alpha{(pixel >> 24u) / 255.0};
                                  total += alpha * (((pixel >> 16u) & 0xffu) + ((pixel >> 8u) & 0xffu) + (pixel & 0xffu)) / 3.0;
                                  covered += alpha;
                              }
                              return total / covered;
                          }};
    oui_paint::Icons icons;
    const auto light{brightness(*icons.picture("Bucket", 16, 16))};
    icons.set_dark_interface(true);
    const auto dark{brightness(*icons.picture("Bucket", 16, 16))};
    EXPECT_GT(dark, light + 60.0) << light << " on a light interface, " << dark << " on a dark one";
}

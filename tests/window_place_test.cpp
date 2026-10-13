#include "app/window_place.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace oui_paint;

TEST(WindowPlace, TheWindowOpensWhereAndAsBigAsItWasLeft)
{
    const auto file{std::filesystem::temp_directory_path() / "oui_paint_window_place_test.ini"};
    std::filesystem::remove(file);
    {
        olagarro::oui::Settings settings{file};
        const auto fresh{window_place_from(settings)};
        EXPECT_FALSE(fresh.position) << "the first time the desktop places it";
        EXPECT_FALSE(fresh.maximized);
        olagarro::oui::WindowGeometry left;
        left.position = olagarro::oui::Point{120, 80};
        left.size = {1500, 900};
        left.maximized = true;
        keep_window_place(settings, left);
        ASSERT_TRUE(settings.save());
    }
    olagarro::oui::Settings again{file};
    ASSERT_TRUE(again.load());
    const auto place{window_place_from(again)};
    ASSERT_TRUE(place.position);
    EXPECT_EQ(place.position->x, 120);
    EXPECT_EQ(place.position->y, 80);
    EXPECT_EQ(place.size.width, 1500);
    EXPECT_EQ(place.size.height, 900);
    EXPECT_TRUE(place.maximized);
    std::filesystem::remove(file);
}

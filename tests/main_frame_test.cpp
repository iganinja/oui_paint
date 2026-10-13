#include "ui/main_frame.h"

#include <oui/label.h>

#include <gtest/gtest.h>

using namespace olagarro;

TEST(MainFrame, MenuOnTopStatusAtTheBottomAndTheCenterBetween)
{
    oui::CommandRegistry commands;
    oui_paint::MainFrame frame{commands};
    frame.menu_bar().add_menu("&File", {});
    frame.set_center(oui::label("middle"));
    frame.arrange({0, 0, 80, 25});

    EXPECT_EQ(frame.menu_bar().bounds().y, 0);
    EXPECT_EQ(frame.menu_bar().bounds().height, 1);
    EXPECT_EQ(frame.status_bar().bounds().y, 24);
    EXPECT_EQ(frame.status_bar().bounds().height, 1);
    ASSERT_NE(frame.center(), nullptr);
    EXPECT_EQ(frame.center()->bounds().y, 1);
    EXPECT_EQ(frame.center()->bounds().height, 23);
    EXPECT_EQ(frame.center()->bounds().width, 80);
}

TEST(MainFrame, ReplacingTheCenterDropsTheOldOne)
{
    oui::CommandRegistry commands;
    oui_paint::MainFrame frame{commands};
    frame.set_center(oui::label("first"));
    frame.set_center(oui::label("second"));
    EXPECT_EQ(frame.child_count(), 3u);
}

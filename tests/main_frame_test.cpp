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

TEST(MainFrame, TheLeftPanelTakesTheWidthItAsksAndTheCenterTheRest)
{
    oui::CommandRegistry commands;
    oui_paint::MainFrame frame{commands};
    frame.menu_bar().add_menu("&File", {});
    frame.set_center(oui::label("middle"));
    frame.set_left(oui::label("left"));
    frame.arrange({0, 0, 80, 25});
    ASSERT_NE(frame.left(), nullptr);
    EXPECT_EQ(frame.left()->bounds().x, 0);
    EXPECT_EQ(frame.left()->bounds().width, 4);
    EXPECT_EQ(frame.left()->bounds().y, 1);
    EXPECT_EQ(frame.left()->bounds().height, 23);
    EXPECT_EQ(frame.center()->bounds().x, 4);
    EXPECT_EQ(frame.center()->bounds().width, 76);
}

TEST(MainFrame, AHiddenRightPanelLeavesItsWidthToTheCenter)
{
    oui::CommandRegistry commands;
    oui_paint::MainFrame frame{commands};
    frame.set_center(oui::label("middle"));
    auto right{oui::label("layers")};
    auto* const panel{right.get()};
    frame.set_right(std::move(right));
    frame.arrange({0, 0, 80, 25});
    EXPECT_EQ(frame.center()->bounds().width, 74);
    panel->set_visible(false);
    frame.arrange({0, 0, 80, 25});
    EXPECT_EQ(frame.center()->bounds().width, 80);
    panel->set_visible(true);
    frame.arrange({0, 0, 80, 25});
    EXPECT_EQ(frame.center()->bounds().width, 74);
}

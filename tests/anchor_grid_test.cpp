#include "ui/anchor_grid.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(AnchorGrid, AClickOrTheArrowKeysChooseTheSquareTheImageStaysIn)
{
    Icons icons;
    AnchorGrid grid{icons};
    grid.arrange({0, 0, 12, 6});
    std::vector<int> chosen;
    grid.on_changed([&chosen](int anchor) { chosen.push_back(anchor); });
    EXPECT_EQ(grid.anchor(), 4) << "the middle to start with";
    grid.handle_event(olagarro::oui::MouseEvent{1, 1, true});
    EXPECT_EQ(grid.anchor(), 0) << "the top left square, four cells by two";
    grid.handle_event(olagarro::oui::KeyEvent{olagarro::oui::Key::right});
    grid.handle_event(olagarro::oui::KeyEvent{olagarro::oui::Key::down});
    grid.handle_event(olagarro::oui::KeyEvent{olagarro::oui::Key::down});
    grid.handle_event(olagarro::oui::KeyEvent{olagarro::oui::Key::down});
    EXPECT_EQ(grid.anchor(), 7) << "the bottom, and no further";
    EXPECT_EQ(chosen, (std::vector<int>{0, 1, 4, 7})) << "heard once for each square changed";
    EXPECT_FALSE(grid.handle_event(olagarro::oui::MouseEvent{20, 1, true})) << "past the squares";
}

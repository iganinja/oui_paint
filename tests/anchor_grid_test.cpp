#include "ui/anchor_grid.h"

#include <oui/application.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/pixel_backend.h>

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

TEST(AnchorGrid, TheSquareUnderThePointerIsLit)
{
    namespace oui = olagarro::oui;
    auto backend{oui::image_backend({480, 240})};
    auto& window{*backend};
    oui::Application application{std::move(backend)};
    Icons icons;
    application.set_content(std::make_unique<AnchorGrid>(icons));
    application.run_frame();
    const auto cell_x{oui::whole(480.0 / window.size().width)};
    // Just inside the edges of the top left square and of the top right one.
    const auto first{[&] { return window.canvas().pixel_at(3, 3); }};
    const auto third{[&] { return window.canvas().pixel_at(8 * cell_x + 3, 3); }};
    const auto resting{first()};
    ASSERT_EQ(third().red, resting.red);
    oui::MouseEvent mouse{1, 1, false};
    mouse.hover = true;
    window.push(mouse);
    application.run_frame();
    const auto lit{first()};
    EXPECT_FALSE(lit.red == resting.red && lit.green == resting.green && lit.blue == resting.blue);
    EXPECT_EQ(third().red, resting.red) << "only the one under the pointer";
}

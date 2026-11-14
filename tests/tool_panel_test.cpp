#include "ui/tool_panel.h"

#include <oui/application.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/pixel_backend.h>

#include <gtest/gtest.h>

using namespace olagarro;

namespace
{

std::vector<oui_paint::ToolButton> three_tools()
{
    return {{"select", "RegionEdit", "Select"}, {"move", "ToolMove", "Move"}, {"zoom", "Zoom", "Zoom"}};
}

} // namespace

TEST(ToolPanel, ButtonsGoTwoToARow)
{
    oui_paint::Icons icons;
    oui_paint::ToolPanel panel{three_tools(), icons};
    EXPECT_EQ(panel.measure({80, 25}).width, 4);
    EXPECT_EQ(panel.measure({80, 25}).height, 2);
    EXPECT_EQ(panel.button_at({0, 0}), 0u);
    EXPECT_EQ(panel.button_at({1, 0}), 0u);
    EXPECT_EQ(panel.button_at({2, 0}), 1u);
    EXPECT_EQ(panel.button_at({0, 1}), 2u);
    EXPECT_FALSE(panel.button_at({2, 1}).has_value());
    EXPECT_FALSE(panel.button_at({4, 0}).has_value());
}

TEST(ToolPanel, AClickPicksTheToolUnderIt)
{
    oui_paint::Icons icons;
    oui_paint::ToolPanel panel{three_tools(), icons};
    panel.arrange({10, 3, 4, 2});
    std::string picked;
    panel.on_picked([&picked](const std::string& id) { picked = id; });
    EXPECT_EQ(panel.selected(), "select");
    EXPECT_TRUE(panel.handle_event(oui::MouseEvent{10, 4, true}));
    EXPECT_EQ(picked, "zoom");
    EXPECT_EQ(panel.selected(), "zoom");
    EXPECT_EQ(panel.tooltip_at({12, 3}), "Move");
}

TEST(ToolPanel, TheButtonUnderThePointerIsLit)
{
    auto backend{oui::image_backend({480, 240})};
    auto& window{*backend};
    oui::Application application{std::move(backend)};
    oui_paint::Icons icons;
    auto panel{std::make_unique<oui_paint::ToolPanel>(three_tools(), icons)};
    application.set_content(std::move(panel));
    application.run_frame();
    // The corner of the second button, which is not the one selected, and of the third.
    const auto corner{[&window](int x, int y) { return window.canvas().pixel_at(x, y); }};
    const auto cell_x{oui::whole(480.0 / window.size().width)};
    const auto cell_y{oui::whole(240.0 / window.size().height)};
    const auto second{[&] { return corner(2 * cell_x + 1, 1); }};
    const auto third{[&] { return corner(1, cell_y + 1); }};
    const auto resting{second()};
    const auto hover{[&application, &window](double x, double y)
                     {
                         oui::MouseEvent mouse{x, y, false};
                         mouse.hover = true;
                         window.push(mouse);
                         application.run_frame();
                     }};
    hover(2.5, 0.5);
    const auto lit{second()};
    EXPECT_FALSE(lit.red == resting.red && lit.green == resting.green && lit.blue == resting.blue);
    EXPECT_EQ(third().red, resting.red) << "only the one under the pointer";
    hover(1.5, 1.5);
    EXPECT_EQ(second().red, resting.red);
    EXPECT_EQ(second().green, resting.green);
}

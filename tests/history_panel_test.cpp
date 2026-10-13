#include "ui/history_panel.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

// A history of `steps` steps, each a dot.
History made(Document& document, int steps)
{
    History history;
    for(int step{}; step < steps; ++ step)
    {
        history.begin_step(document);
        Image pixel{PixelFormat::gray_alpha8, 1, 1};
        pixel.set_pixel(0, 0, GrayAlpha8{{static_cast<std::uint8_t>(step + 1), 255}});
        document.layers().front().write(pixel, step, 0);
        history.finish_step("Dot " + std::to_string(step + 1), document);
    }
    return history;
}

olagarro::oui::MouseEvent click(int x, int y)
{
    return olagarro::oui::MouseEvent{x, y, true};
}

} // namespace

TEST(HistoryPanel, ARowIsThePositionItTakesTheImageToTheFirstAsItWas)
{
    Document document{16, 16, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    auto history{made(document, 3)};
    Icons icons;
    HistoryPanel panel{icons};
    panel.set_history(&history);
    panel.arrange({0, 0, 24, 10});
    EXPECT_EQ(panel.position_at_row(0), std::optional<std::size_t>{0}) << "the image as it was first";
    EXPECT_EQ(panel.position_at_row(3), std::optional<std::size_t>{3});
    EXPECT_EQ(panel.position_at_row(4), std::nullopt) << "past the steps";
    EXPECT_EQ(panel.position_at_row(9), std::nullopt) << "the buttons' row";
    std::optional<std::size_t> chosen;
    panel.on_choose([&chosen](std::size_t position) { chosen = position; });
    panel.handle_event(click(5, 1));
    EXPECT_EQ(chosen, std::optional<std::size_t>{1});
    auto undone{false};
    panel.on_undo([&undone] { undone = true; });
    panel.handle_event(click(0, 9));
    EXPECT_TRUE(undone);
}

TEST(HistoryPanel, ItScrollsSoTheStepItIsAtShows)
{
    Document document{64, 16, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    auto history{made(document, 20)};
    Icons icons;
    HistoryPanel panel{icons};
    panel.set_history(&history);
    panel.arrange({0, 0, 24, 6});
    // Five rows for steps: the last one, 20, at the bottom.
    EXPECT_EQ(panel.position_at_row(4), std::optional<std::size_t>{20});
    EXPECT_EQ(panel.position_at_row(0), std::optional<std::size_t>{16});
    static_cast<void>(history.go_to(document, 2));
    EXPECT_EQ(panel.position_at_row(0), std::optional<std::size_t>{2}) << "gone back, it follows";
}

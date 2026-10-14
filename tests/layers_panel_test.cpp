#include "ui/layers_panel.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

olagarro::oui::MouseEvent click(int x, int y)
{
    return olagarro::oui::MouseEvent{x, y, true};
}

} // namespace

TEST(LayersPanel, ARowTicksAtTheLeftAndShowsOrDeletesAtTheRight)
{
    Document document{8, 8, PixelFormat::rgba8};
    for(const auto* const name : {"A", "B", "C"})
    {
        document.add_layer(name);
    }
    Icons icons;
    LayersPanel panel{icons};
    panel.set_document(&document);
    panel.arrange({0, 0, LayersPanel::width, 10});
    std::optional<std::size_t> deleted;
    std::optional<std::size_t> toggled;
    std::optional<std::size_t> selected;
    panel.on_delete([&deleted](std::size_t index) { deleted = index; });
    panel.on_toggle_visible([&toggled](std::size_t index) { toggled = index; });
    panel.on_select([&selected](std::size_t index) { selected = index; });

    // The top row is the top layer, C.
    panel.handle_event(click(0, 0));
    panel.handle_event(click(1, 2));
    EXPECT_EQ(panel.ticked(), (std::vector<std::size_t>{0, 2})) << "the box ticks, bottom first";
    panel.handle_event(click(0, 0));
    EXPECT_EQ(panel.ticked(), (std::vector<std::size_t>{0})) << "and unticks";
    panel.handle_event(click(LayersPanel::width - 1, 1));
    EXPECT_EQ(deleted, std::optional<std::size_t>{1}) << "the bin, at the right";
    panel.handle_event(click(LayersPanel::width - 3, 1));
    EXPECT_EQ(toggled, std::optional<std::size_t>{1}) << "the eye, beside it";
    panel.handle_event(click(8, 2));
    EXPECT_EQ(selected, std::optional<std::size_t>{0}) << "the name selects";

    document.add_layer("D");
    EXPECT_TRUE(panel.ticked().empty()) << "a layer added: the ticks are gone, the places changed";
}

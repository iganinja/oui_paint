#include "ui/layers_panel.h"
#include "ui/side_panel.h"

#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>

#include <gtest/gtest.h>

#include <string>

using namespace oui_paint;

namespace
{

olagarro::oui::MouseEvent click(double x, double y)
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

TEST(LayersPanel, InAWindowThePointerFindsWhatIsDrawnUnderIt)
{
    // A window places the panel between cells and gives the pointer between them too.
    Document document{8, 8, PixelFormat::rgba8};
    for(const auto* const name : {"A", "B", "C"})
    {
        document.add_layer(name);
    }
    Icons icons;
    LayersPanel panel{icons};
    panel.set_document(&document);
    panel.arrange({0.5, 0.25, 23.5, 10.5});
    std::optional<LayersPanel::Action> action;
    std::optional<std::size_t> deleted;
    std::optional<std::size_t> toggled;
    panel.on_action([&action](LayersPanel::Action done) { action = done; });
    panel.on_delete([&deleted](std::size_t index) { deleted = index; });
    panel.on_toggle_visible([&toggled](std::size_t index) { toggled = index; });

    // The buttons' bar is the last row, from 9.75 to 10.75.
    panel.handle_event(click(0.8, 10.5));
    EXPECT_EQ(action, std::optional{LayersPanel::Action::add}) << "the first button, below the middle of its row";
    action.reset();
    panel.handle_event(click(2.7, 9.9));
    EXPECT_EQ(action, std::optional{LayersPanel::Action::remove}) << "the second, above it";
    // The eye from 20 to 22 and the bin from 22 to 24, as the panel ends at 24.
    panel.handle_event(click(20.2, 1.5));
    EXPECT_EQ(toggled, std::optional<std::size_t>{1}) << "the eye where it starts, in the column before";
    panel.handle_event(click(22.3, 1.5));
    EXPECT_EQ(deleted, std::optional<std::size_t>{1}) << "the bin where it starts, in the column before";
}

TEST(LayersPanel, InAWindowNoLayerIsDrawnOrChosenUnderTheButtons)
{
    // In 10.5 rows the buttons take the last, from 9.5: nine layers fit above them, not ten.
    Document document{8, 8, PixelFormat::rgba8};
    for(int layer{}; layer < 12; ++ layer)
    {
        document.add_layer("Layer " + std::to_string(layer));
    }
    Icons icons;
    LayersPanel panel{icons};
    panel.set_document(&document);
    panel.arrange({0, 0, LayersPanel::width, 10.5});
    // The tenth row from the top is layer 2.
    document.set_active_layer(2);
    auto cell{olagarro::oui::default_cell_font()};
    auto interface{olagarro::oui::default_interface_font()};
    auto symbols{olagarro::oui::symbol_font(12, 24, cell->ascent())};
    const auto style{olagarro::oui::classic_style()};
    olagarro::oui::Canvas canvas{12 * LayersPanel::width, 252};
    olagarro::oui::GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    const olagarro::oui::Theme theme;
    ASSERT_NE(side_panel_color(theme), theme.menu_selected_background);
    panel.paint(painter, theme);
    // Row 9 would be pixels 216 to 240, the buttons' bar 228 to 252; pixel 66 across is the column between the
    // layer's picture and its name, where only the row's colour is.
    EXPECT_NE(canvas.pixel_at(66, 222), theme.menu_selected_background) << "nothing of the tenth row shows";
    document.set_active_layer(3);
    panel.paint(painter, theme);
    EXPECT_EQ(canvas.pixel_at(66, 200), theme.menu_selected_background) << "the ninth does";

    std::optional<std::size_t> selected;
    panel.on_select([&selected](std::size_t index) { selected = index; });
    panel.handle_event(click(8, 9.2));
    EXPECT_EQ(selected, std::nullopt) << "where no row is drawn, none is chosen";
}

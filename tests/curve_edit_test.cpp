#include "effects/curve_edit.h"
#include "ui/curve_editor.h"

#include <oui/event.h>
#include <oui/graphics/canvas.h>
#include <oui/graphics/font.h>
#include <oui/graphics/graphical_painter.h>

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(CurveEdit, APressAddsAPointThereAndADragMovesItBetweenItsNeighbours)
{
    CurveEdit edit;
    EXPECT_TRUE(edit.press(0.5, 0.7, false));
    ASSERT_EQ(edit.points().size(), 3u);
    EXPECT_EQ(edit.points()[1], (CurvePoint{0.5, 0.7}));
    EXPECT_EQ(edit.held(), std::optional<std::size_t>{1});
    EXPECT_TRUE(edit.drag(1.2, 0.3)) << "past the last point, but not far enough to take it away";
    EXPECT_NEAR(edit.points()[1].x, 1.0 - 1.0 / 255.0, 1e-12) << "kept before it";
    EXPECT_EQ(edit.points()[1].y, 0.3);
    edit.release();
    EXPECT_FALSE(edit.press(0.0, 0.01, false)) << "near the first point: takes it, adds nothing";
    EXPECT_EQ(edit.points().size(), 3u);
    EXPECT_TRUE(edit.drag(0.1, 0.2));
    EXPECT_EQ(edit.points().front(), (CurvePoint{0.1, 0.2})) << "the ends move too";
}

TEST(CurveEdit, APointGoesWithARightPressOrDraggedFarOffButTwoStay)
{
    CurveEdit edit;
    edit.set_points({{0, 0}, {0.5, 0.5}, {1, 1}});
    EXPECT_TRUE(edit.press(0.5, 0.5, true));
    EXPECT_EQ(edit.points().size(), 2u);
    EXPECT_FALSE(edit.press(0, 0, true)) << "two are the fewest";
    edit.set_points({{0, 0}, {0.5, 0.5}, {1, 1}});
    edit.press(0.5, 0.5, false);
    EXPECT_TRUE(edit.drag(0.5, 1.4)) << "far above the graph";
    EXPECT_EQ(edit.points().size(), 2u);
    EXPECT_TRUE(edit.drag(0.5, 0.9)) << "back on it";
    ASSERT_EQ(edit.points().size(), 3u);
    EXPECT_EQ(edit.points()[1], (CurvePoint{0.5, 0.9}));
}

TEST(CurveEditor, InAWindowBetweenCellsAPointIsDrawnUnderThePixelPressed)
{
    // Placed between cells, as a window places it: from pixel 18, 6 to 234, 294 in cells of 12 by 24.
    auto cell{olagarro::oui::default_cell_font()};
    auto interface{olagarro::oui::default_interface_font()};
    auto symbols{olagarro::oui::symbol_font(12, 24, cell->ascent())};
    const auto style{olagarro::oui::classic_style()};
    olagarro::oui::Canvas canvas{260, 300};
    olagarro::oui::GraphicalPainter painter{canvas, style, *cell, *interface, *symbols};
    CurveEditor editor;
    editor.arrange({1.5, 0.25, 18, 12});
    editor.paint(painter, {});
    const olagarro::oui::Point pressed{101, 117};
    olagarro::oui::MouseEvent mouse{pressed.x / 12, pressed.y / 24, true};
    mouse.pixel = pressed;
    EXPECT_TRUE(editor.handle_event(mouse));
    ASSERT_EQ(editor.points().size(), 3u);
    editor.paint(painter, {});
    // The point held is drawn in amber, its middle where it was pressed.
    EXPECT_EQ(canvas.pixel_at(101, 117), (olagarro::oui::Color{0xE4, 0xA8, 0x2C}));
    EXPECT_NE(canvas.pixel_at(101 + 6, 117), (olagarro::oui::Color{0xE4, 0xA8, 0x2C})) << "and no further";
    EXPECT_NE(canvas.pixel_at(101, 117 - 6), (olagarro::oui::Color{0xE4, 0xA8, 0x2C}));
}

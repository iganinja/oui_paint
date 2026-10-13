#include "core/history.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

void dot(Document& document, int x, int y, std::uint8_t value)
{
    Image pixel{PixelFormat::gray_alpha8, 1, 1};
    pixel.set_pixel(0, 0, GrayAlpha8{{value, 255}});
    document.layers()[document.active_layer()].write(pixel, x, y);
}

std::uint8_t value_at(const Document& document, int x, int y)
{
    return document.flatten().pixel<GrayAlpha8>(x, y).value[0];
}

} // namespace

TEST(History, UndoAndRedoPutTheLayersBackAndSayWhichTilesChanged)
{
    Document document{600, 300, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    History history;
    history.begin_step(document);
    dot(document, 10, 10, 50);
    history.finish_step("First", document);
    history.begin_step(document);
    dot(document, 500, 10, 90);
    history.finish_step("Second", document);
    ASSERT_EQ(history.steps().size(), 2u);

    const auto changed{history.undo(document)};
    EXPECT_EQ(changed, (std::vector<TilePoint>{{1, 0}})) << "only the tile the step wrote";
    EXPECT_EQ(value_at(document, 500, 10), 0);
    EXPECT_EQ(value_at(document, 10, 10), 50);
    history.undo(document);
    EXPECT_EQ(value_at(document, 10, 10), 0);
    EXPECT_FALSE(history.can_undo());
    history.redo(document);
    history.redo(document);
    EXPECT_EQ(value_at(document, 500, 10), 90);
    EXPECT_FALSE(history.can_redo());
}

TEST(History, ANewStepDropsWhatCouldBeRedoneAndAStepThatChangedNothingIsNotKept)
{
    Document document{10, 10, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    History history;
    history.begin_step(document);
    dot(document, 1, 1, 10);
    history.finish_step("One", document);
    history.undo(document);
    history.begin_step(document);
    dot(document, 2, 2, 20);
    history.finish_step("Two", document);
    EXPECT_EQ(history.steps().size(), 1u);
    EXPECT_EQ(history.steps()[0].name, "Two");
    history.begin_step(document);
    history.finish_step("Nothing", document);
    EXPECT_EQ(history.steps().size(), 1u);
}

TEST(History, AStepKeepsOnlyTheTilesItWrote)
{
    Document document{1024, 1024, PixelFormat::gray_alpha8};
    auto& layer{document.add_layer("Background")};
    Image full{PixelFormat::gray_alpha8, 1024, 1024};
    layer.write(full, 0, 0);
    History history;
    history.begin_step(document);
    dot(document, 1, 1, 10);
    history.finish_step("Dot", document);
    const auto& step{history.steps().front()};
    auto shared{0};
    for(const auto& [point, tile] : step.before.layers[0].tiles())
    {
        shared += step.after.layers[0].tile(point) == tile ? 1 : 0;
    }
    EXPECT_EQ(shared, 15) << "of 16 tiles, the 15 not written are the same in both";
}

TEST(History, TheSelectionIsUndoneWithTheRest)
{
    Document document{50, 50, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    History history;
    history.begin_step(document);
    document.selection().add_rectangle({5, 5, 10, 10}, SelectionMode::replace);
    history.finish_step("Rectangle Select", document);
    ASSERT_EQ(history.steps().size(), 1u) << "a step that only selects is kept";
    history.undo(document);
    EXPECT_FALSE(document.selection().active());
    history.redo(document);
    EXPECT_EQ(document.selection().at(7, 7), 255);
}

TEST(History, AGrownCanvasShrinksBackOnUndo)
{
    Document document{50, 40, PixelFormat::rgba8};
    document.add_layer("Background");
    History history;
    history.begin_step(document);
    document.set_size(300, 200);
    history.finish_step("Expand Canvas", document);
    ASSERT_EQ(history.steps().size(), 1u);
    history.undo(document);
    EXPECT_EQ(document.width(), 50);
    EXPECT_EQ(document.selection().width(), 50);
    history.redo(document);
    EXPECT_EQ(document.height(), 200);
}

TEST(History, GoingToAStepUndoesOrRedoesAsManyAsItTakesAtOnce)
{
    Document document{600, 300, PixelFormat::gray_alpha8};
    document.add_layer("Background");
    History history;
    for(const std::uint8_t value : {10, 20, 30})
    {
        history.begin_step(document);
        dot(document, value, 10, value);
        history.finish_step("Dot", document);
    }
    const auto back{history.go_to(document, 1)};
    EXPECT_EQ(history.position(), 1u);
    EXPECT_EQ(value_at(document, 10, 10), 10);
    EXPECT_EQ(value_at(document, 20, 10), 0);
    EXPECT_EQ(value_at(document, 30, 10), 0);
    EXPECT_EQ(back, (std::vector<TilePoint>{{0, 0}})) << "each tile once";
    static_cast<void>(history.go_to(document, 3));
    EXPECT_EQ(history.position(), 3u);
    EXPECT_EQ(value_at(document, 30, 10), 30);
    static_cast<void>(history.go_to(document, 0));
    EXPECT_EQ(value_at(document, 10, 10), 0) << "the image as it opened";
    EXPECT_THROW(static_cast<void>(history.go_to(document, 9)), std::out_of_range) << "no such step";
}

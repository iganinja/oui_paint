#include "app/open_document.h"
#include "app/paint_app.h"

#include <oui/application.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/settings.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>

using namespace olagarro;
using namespace oui_paint;

namespace
{

[[nodiscard]] std::filesystem::path make_folder()
{
    static auto count{0};
    const auto* const test{::testing::UnitTest::GetInstance()->current_test_info()};
    auto folder{std::filesystem::temp_directory_path() /
                ("oui_paint_app_test_" + std::string{test->name()} + "_" + std::to_string(++ count))};
    std::filesystem::create_directories(folder);
    return folder;
}

// OUI Paint whole, in a window painted in memory, its settings in a file of the test's own.
class PaintAppTest : public ::testing::Test
{
  protected:
    PaintAppTest()
    {
        auto backend{oui::image_backend({1000, 700})};
        m_window = backend.get();
        m_application = std::make_unique<oui::Application>(std::move(backend));
        m_paint = std::make_unique<PaintApp>(*m_application, *m_window, m_settings);
    }
    ~PaintAppTest() override
    {
        m_paint.reset();
        m_application.reset();
        std::error_code ignored;
        std::filesystem::remove_all(m_folder, ignored);
    }

    // A new image of `width` by `height`, one layer, in a tab of its own, the current one.
    OpenDocument& add_image(int width, int height)
    {
        // With a layer, as New Image makes it.
        Document document{width, height, PixelFormat::rgba8};
        static_cast<void>(document.add_layer("Background"));
        m_paint->add(std::make_unique<OpenDocument>(std::move(document)));
        m_application->render();
        return *m_paint->current();
    }

    // The pointer pressed at `from` in the image's pixels, dragged to `to` and let go there, on the current image's canvas.
    void drag(Position from, Position to)
    {
        m_application->render();
        auto* const canvas{m_application->focused()};
        ASSERT_NE(canvas, nullptr);
        const auto cells{m_window->size()};
        const auto cell_width{1000.0 / cells.width};
        const auto cell_height{700.0 / cells.height};
        const auto& viewport{m_paint->current()->viewport()};
        const auto event{[&](Position at, bool pressed, bool dragging, bool released)
                         {
                             const auto view{viewport.view_at(at)};
                             const oui::Point pixel{canvas->bounds().x * cell_width + view.x - 0.5,
                                                    canvas->bounds().y * cell_height + view.y - 0.5};
                             oui::MouseEvent mouse{pixel.x / cell_width, pixel.y / cell_height, pressed};
                             mouse.dragging = dragging;
                             mouse.released = released;
                             mouse.pixel = pixel;
                             return oui::Event{mouse};
                         }};
        m_application->dispatch_event(event(from, true, false, false));
        m_application->dispatch_event(event(to, false, true, false));
        m_application->dispatch_event(event(to, false, false, true));
        m_application->render();
    }

    // How opaque a pixel of the current image's active layer is.
    [[nodiscard]] int alpha(int x, int y)
    {
        const auto& document{m_paint->current()->document()};
        return document.layers()[document.active_layer()].read(x, y, 1, 1).bytes.at(3);
    }

    [[nodiscard]] bool has_shortcut(std::string_view command, const oui::KeyShortcut& wanted) const
    {
        const auto shortcuts{m_application->commands().shortcuts(command)};
        return std::any_of(shortcuts.begin(), shortcuts.end(),
                           [&wanted](const oui::Shortcut& shortcut)
                           {
                               return shortcut.key == wanted.key &&
                                      shortcut.control == wanted.control && shortcut.alt == wanted.alt &&
                                      shortcut.shift == wanted.shift;
                           });
    }

    // A folder of the test's own, so the plugins beside the settings are none of the person's.
    std::filesystem::path m_folder{make_folder()};
    std::filesystem::path m_settings_path{m_folder / "oui_paint.ini"};
    oui::Settings m_settings{m_settings_path};
    oui::ImageBackend* m_window{};
    std::unique_ptr<oui::Application> m_application;
    std::unique_ptr<PaintApp> m_paint;
};

TEST_F(PaintAppTest, CopiesCutsAndPastesWithTheInsertAndDeleteKeysToo)
{
    EXPECT_TRUE(has_shortcut("edit.copy", oui::KeyShortcut{oui::Key::insert_, true}));
    EXPECT_TRUE(has_shortcut("edit.cut", oui::KeyShortcut{oui::Key::delete_, false, false, true}));
    EXPECT_TRUE(has_shortcut("edit.paste", oui::KeyShortcut{oui::Key::insert_, false, false, true}));
    // Ctrl+C, X and V stay the first, the ones the menu shows.
    const auto copy{m_application->commands().shortcut("edit.copy")};
    ASSERT_TRUE(copy.has_value());
    EXPECT_EQ(copy->character, U'c');
    EXPECT_TRUE(copy->control);
}

TEST_F(PaintAppTest, TheMenuShowsCtrlAltPlusForABiggerInterface)
{
    // Where = is Shift and 0, as on a Spanish keyboard, Ctrl+Alt+= would be Ctrl+Alt+0, which sets it as it starts.
    const auto bigger{m_application->commands().shortcut("view.interface_bigger")};
    ASSERT_TRUE(bigger.has_value());
    EXPECT_EQ(bigger->character, U'+');
    EXPECT_TRUE(bigger->control);
    EXPECT_TRUE(bigger->alt);
}

TEST_F(PaintAppTest, TheArrowKeysMoveTheSelectionAPixelOrMoreWithShiftOrCtrl)
{
    auto& image{add_image(200, 100)};
    auto& selection{image.document().selection()};
    selection.add_rectangle({10, 20, 5, 5}, SelectionMode::replace);
    // M, then M again: Move Selected Pixels, then Move Selection.
    ASSERT_TRUE(m_application->commands().execute("tool.key_m"));
    ASSERT_TRUE(m_application->commands().execute("tool.key_m"));
    const auto press{[this](oui::Key key, bool shift, bool control)
                     { m_application->dispatch_event(oui::KeyEvent{key, control, false, shift}); }};
    press(oui::Key::right, false, false);
    EXPECT_EQ(selection.bounds().x, 11);
    press(oui::Key::down, true, false);
    EXPECT_EQ(selection.bounds().y, 30) << "ten with Shift";
    press(oui::Key::right, false, true);
    EXPECT_EQ(selection.bounds().x, 61) << "fifty with Ctrl";
    press(oui::Key::left, false, false);
    press(oui::Key::up, false, false);
    EXPECT_EQ(selection.bounds().x, 60);
    EXPECT_EQ(selection.bounds().y, 29);
    EXPECT_EQ(selection.bounds().width, 5);
    // Each one a step of its own, undone as one.
    ASSERT_TRUE(m_application->commands().execute("edit.undo"));
    EXPECT_EQ(selection.bounds().y, 30);
}

TEST_F(PaintAppTest, TheStepsWithShiftAndCtrlAreThePreferences)
{
    m_settings.set_int_value("arrow_shift_step", 3);
    m_settings.set_int_value("arrow_control_step", 7);
    auto& image{add_image(100, 100)};
    auto& selection{image.document().selection()};
    selection.add_rectangle({10, 10, 5, 5}, SelectionMode::replace);
    ASSERT_TRUE(m_application->commands().execute("tool.key_m"));
    ASSERT_TRUE(m_application->commands().execute("tool.key_m"));
    m_application->dispatch_event(oui::KeyEvent{oui::Key::right, false, false, true});
    m_application->dispatch_event(oui::KeyEvent{oui::Key::down, true});
    EXPECT_EQ(selection.bounds().x, 13);
    EXPECT_EQ(selection.bounds().y, 17);
}

TEST_F(PaintAppTest, TheArrowKeysMoveTheSelectedPixelsWithMoveSelectedPixels)
{
    auto& image{add_image(100, 100)};
    auto& selection{image.document().selection()};
    selection.add_rectangle({10, 10, 5, 5}, SelectionMode::replace);
    ASSERT_TRUE(m_application->commands().execute("tool.key_m"));
    m_application->dispatch_event(oui::KeyEvent{oui::Key::left});
    m_application->dispatch_event(oui::KeyEvent{oui::Key::left});
    EXPECT_EQ(selection.bounds().x, 8) << "the selection goes with the pixels";
    EXPECT_TRUE(image.modified());
}

TEST_F(PaintAppTest, TheArrowKeysMoveNothingWithOtherTools)
{
    auto& image{add_image(100, 100)};
    auto& selection{image.document().selection()};
    selection.add_rectangle({10, 10, 5, 5}, SelectionMode::replace);
    m_application->dispatch_event(oui::KeyEvent{oui::Key::right});
    EXPECT_EQ(selection.bounds().x, 10);
}

TEST_F(PaintAppTest, AShapeDrawnStaysToMoveAndSizeUntilItIsDoneWith)
{
    auto& image{add_image(100, 100)};
    // O, then O again: Line / Curve, then Shapes, an outlined rectangle unless told otherwise.
    ASSERT_TRUE(m_application->commands().execute("tool.key_o"));
    ASSERT_TRUE(m_application->commands().execute("tool.key_o"));
    drag({10.5, 10.5}, {30.5, 30.5});
    EXPECT_GT(alpha(10, 20), 0) << "its left edge";
    EXPECT_EQ(alpha(20, 20), 0) << "outlined, not filled";
    EXPECT_FALSE(image.modified()) << "still being edited";

    // A drag inside it moves it.
    drag({20.5, 20.5}, {40.5, 20.5});
    EXPECT_EQ(alpha(10, 20), 0);
    EXPECT_GT(alpha(30, 20), 0);
    // Its bottom right handle sizes it.
    drag({50.5, 30.5}, {60.5, 40.5});
    EXPECT_GT(alpha(60, 35), 0) << "the right edge, further";
    EXPECT_GT(alpha(45, 40), 0) << "the bottom edge, lower";
    EXPECT_GT(alpha(30, 20), 0) << "the left edge where it was";

    // Enter leaves it as it is, one step.
    m_application->dispatch_event(oui::KeyEvent{oui::Key::enter});
    EXPECT_TRUE(image.modified());
    ASSERT_TRUE(m_application->commands().execute("edit.undo"));
    EXPECT_EQ(alpha(30, 20), 0);
    EXPECT_EQ(alpha(60, 35), 0);
    EXPECT_EQ(alpha(10, 20), 0) << "undone whole, moves and all";
}

TEST_F(PaintAppTest, AShapeIsDoneWithOnAnotherToolCtrlDOrAPressOutsideIt)
{
    auto& image{add_image(100, 100)};
    ASSERT_TRUE(m_application->commands().execute("tool.key_o"));
    ASSERT_TRUE(m_application->commands().execute("tool.key_o"));
    drag({10.5, 10.5}, {20.5, 20.5});
    // A press outside it leaves it and draws another, which a drag inside the first then does not move.
    drag({60.5, 60.5}, {80.5, 80.5});
    drag({15.5, 15.5}, {35.5, 15.5});
    EXPECT_GT(alpha(10, 15), 0) << "the first stays where it was";
    EXPECT_GT(alpha(60, 70), 0) << "the second too";
    EXPECT_TRUE(image.modified());
    ASSERT_TRUE(m_application->commands().execute("edit.deselect"));
    // Each shape one step: undone, the deselection, the second shape, then the first.
    ASSERT_TRUE(m_application->commands().execute("edit.undo"));
    EXPECT_GT(alpha(60, 70), 0);
    ASSERT_TRUE(m_application->commands().execute("edit.undo"));
    EXPECT_EQ(alpha(60, 70), 0);
    EXPECT_GT(alpha(10, 15), 0);

    drag({60.5, 60.5}, {80.5, 80.5});
    ASSERT_TRUE(m_application->commands().execute("tool.key_b"));
    drag({70.5, 70.5}, {90.5, 70.5});
    EXPECT_GT(alpha(60, 70), 0) << "another tool left it as it was";
}

TEST_F(PaintAppTest, EnterLeavesALineAsItIsAfterItWasBent)
{
    auto& image{add_image(100, 100)};
    ASSERT_TRUE(m_application->commands().execute("tool.key_o"));
    drag({10.5, 50.5}, {70.5, 50.5});
    // Its first handle, a third along it, pulled up.
    drag({30.5, 50.5}, {30.5, 30.5});
    EXPECT_FALSE(image.modified());
    m_application->dispatch_event(oui::KeyEvent{oui::Key::enter});
    EXPECT_TRUE(image.modified()) << "the bend kept the keys the line takes";
}

} // namespace

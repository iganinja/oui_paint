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

} // namespace

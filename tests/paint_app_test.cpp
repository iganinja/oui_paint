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

    // A new image of `width` by `height` in a tab of its own, the current one.
    OpenDocument& add_image(int width, int height)
    {
        m_paint->add(std::make_unique<OpenDocument>(Document{width, height, PixelFormat::rgba8}));
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

} // namespace

#include "app/recent_files.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(RecentFiles, TheNewestComesFirstOnceAndTheOldestGo)
{
    std::vector<std::string> recent;
    for(const auto* const path : {"a.png", "b.png", "c.png", "a.png"})
    {
        recent = with_recent(recent, path, 3);
    }
    EXPECT_EQ(recent, (std::vector<std::string>{"a.png", "c.png", "b.png"}));
    recent = with_recent(recent, "d.png", 3);
    EXPECT_EQ(recent, (std::vector<std::string>{"d.png", "a.png", "c.png"}));
}

TEST(RecentFiles, AFolderIsShortenedForAMenu)
{
    EXPECT_EQ(short_folder("/home/ana/maps", "/home/ana"), "~/maps");
    EXPECT_EQ(short_folder("/home/anabel/maps", "/home/ana"), "/home/anabel/maps");
    const auto shown{short_folder("/home/ana/projects/game/assets/terrain/heightmaps/winter", "/home/ana", 30)};
    EXPECT_EQ(shown.size(), 30u);
    EXPECT_EQ(shown.rfind("~/pro", 0), 0u);
    EXPECT_EQ(shown.substr(shown.size() - 6), "winter");
}

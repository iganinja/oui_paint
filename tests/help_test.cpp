#include "app/help.h"
#include "app/translations.h"

#include <gtest/gtest.h>

#include <regex>

using namespace oui_paint;

TEST(Help, TheKeysPageListsTheToolsLettersAndEveryBoundCommandUnderItsMenu)
{
    olagarro::oui::CommandRegistry commands;
    commands.register_command("file.save", "&Save", [] {});
    commands.bind("file.save", olagarro::oui::Shortcut{U's', true});
    commands.register_command("layers.add", "&Add New Layer", [] {});
    const auto pages{help_pages(commands)};
    ASSERT_EQ(pages.size(), 3u);
    const auto& keys{pages[1].markdown};
    EXPECT_NE(keys.find("| S | Rectangle Select, then Lasso Select, then Ellipse Select, then Magic Wand |"),
              std::string::npos)
        << keys;
    EXPECT_NE(keys.find("## File\n\n| Key | Command |\n|---|---|\n| Ctrl+S | Save |"), std::string::npos) << keys;
    EXPECT_EQ(keys.find("Add New Layer"), std::string::npos) << "a command without a key is not a key";
    EXPECT_EQ(keys.find("## Layers"), std::string::npos) << "nor makes a heading of nothing";
}

TEST(Help, EveryLinkOfTheGuideLeadsToAPageOrOutOfTheHelp)
{
    const auto pages{help_pages(olagarro::oui::CommandRegistry{})};
    for(const auto& page : pages)
    {
        ASSERT_FALSE(page.markdown.empty()) << page.title;
    }
    EXPECT_EQ(pages[0].markdown.rfind("# OUI Paint", 0), 0u);
    const std::regex link{R"(\]\(([^)]*)\))"};
    const auto& guide{pages[0].markdown};
    auto links{0};
    for(std::sregex_iterator found{guide.begin(), guide.end(), link}; found != std::sregex_iterator{}; ++ found)
    {
        const auto target{(*found)[1].str()};
        ++ links;
        EXPECT_TRUE(help_place(pages, target) || target.rfind("https://", 0) == 0) << target;
    }
    EXPECT_GT(links, 0);
}

TEST(Help, ALinkNamesAPageAndMaybeASectionOfIt)
{
    const auto pages{help_pages(olagarro::oui::CommandRegistry{})};
    const auto plugins{help_place(pages, "plugins.md#after-saving")};
    ASSERT_TRUE(plugins);
    EXPECT_EQ(plugins->page, 2u);
    EXPECT_EQ(plugins->anchor, "after-saving");
    EXPECT_EQ(help_place(pages, "guide.md")->anchor, "");
    EXPECT_FALSE(help_place(pages, "https://example.com/plugins.md"));
}

TEST(Help, AboutSaysTheVersionAndTheLicenseAndTheNoticesNameEveryPiece)
{
    EXPECT_FALSE(version().empty());
    const auto about{about_markdown()};
    EXPECT_NE(about.find("# OUI Paint " + std::string{version()}), std::string::npos);
    EXPECT_NE(about.find("This software is provided 'as-is'"), std::string::npos) << "the zlib license itself";
    const auto notices{third_party_notices()};
    EXPECT_EQ(notices.find("## ?"), std::string::npos) << "a license embedded without its name";
    for(const auto* const piece : {"QuickJS-ng", "NanoSVG", "LodePNG", "miniz", "SDL3", "Godot editor icons"})
    {
        EXPECT_NE(notices.find(std::string{"## "} + piece), std::string::npos) << piece;
    }
    EXPECT_NE(notices.find("Permission is hereby granted"), std::string::npos) << "an MIT license's text";
}

TEST(Help, InSpanishTheGuideAndTheKeysAreInSpanishAndItsLinksLeadToPages)
{
    static_cast<void>(install_paint_language("es"));
    olagarro::oui::CommandRegistry commands;
    commands.register_command("file.save", olagarro::oui::translate("Commands", "&Save"), [] {});
    commands.bind("file.save", olagarro::oui::Shortcut{U's', true});
    const auto pages{help_pages(commands)};
    olagarro::oui::remove_translations();
    ASSERT_EQ(pages.size(), 3u);
    EXPECT_EQ(pages[0].title, "Guía");
    EXPECT_NE(pages[0].markdown.find("## La ventana"), std::string::npos);
    EXPECT_NE(pages[1].markdown.find("## Archivo\n\n| Tecla | Orden |\n|---|---|\n| Ctrl+S | Guardar |"), std::string::npos)
        << pages[1].markdown;
    EXPECT_NE(pages[2].markdown.find("# Plugins"), std::string::npos) << "the plugins' page, which has no translation, in English";
    const std::regex link{R"(\]\(([^)]*)\))"};
    const auto& guide{pages[0].markdown};
    for(std::sregex_iterator found{guide.begin(), guide.end(), link}; found != std::sregex_iterator{}; ++ found)
    {
        EXPECT_TRUE(help_place(pages, (*found)[1].str())) << (*found)[1].str();
    }
    EXPECT_EQ(help_pages(commands)[0].title, "Guide") << "and in English once the catalogs are gone";
}

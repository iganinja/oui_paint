#include "app/fonts.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(Fonts, EachFamilyOnceWithItsRegularFaceInOrder)
{
    const auto families{parse_font_list("/f/DejaVuSerif-Bold.ttf: DejaVu Serif:style=Bold\n"
                                        "/f/DejaVuSerif.ttf: DejaVu Serif:style=Book\n"
                                        "/f/Ubuntu-Light.ttf: Ubuntu,Ubuntu Light:style=Light,Regular\n"
                                        "/f/terminus.pcf.gz: Terminus:style=Regular\n"
                                        "/f/Arimo-Italic.ttf: Arimo:style=Italic\n")};
    ASSERT_EQ(families.size(), 3u);
    EXPECT_EQ(families[0].name, "Arimo");
    EXPECT_EQ(families[1].name, "DejaVu Serif");
    EXPECT_EQ(families[1].path, "/f/DejaVuSerif.ttf");
    EXPECT_EQ(families[2].name, "Ubuntu");
}

TEST(Fonts, AFamilyKeepsItsBoldItalicAndBoldItalicFacesWhereItHasThem)
{
    const auto families{parse_font_list("/f/Sans-Bold.ttf: Sans:style=Bold\n"
                                        "/f/Sans.ttf: Sans:style=Regular\n"
                                        "/f/Sans-Oblique.ttf: Sans:style=Oblique\n"
                                        "/f/Sans-BoldItalic.ttf: Sans:style=Bold Italic\n"
                                        "/f/Sans-ExtraBold.ttf: Sans:style=ExtraBold\n"
                                        "/f/Serif.ttf: Serif:style=Regular\n")};
    ASSERT_EQ(families.size(), 2u);
    EXPECT_EQ(families[0].path, "/f/Sans.ttf");
    EXPECT_EQ(families[0].bold, "/f/Sans-Bold.ttf");
    EXPECT_EQ(families[0].italic, "/f/Sans-Oblique.ttf");
    EXPECT_EQ(families[0].bold_italic, "/f/Sans-BoldItalic.ttf");
    EXPECT_TRUE(families[1].bold.empty());
    EXPECT_TRUE(families[1].italic.empty());
    EXPECT_TRUE(families[1].bold_italic.empty());
}

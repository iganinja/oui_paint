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

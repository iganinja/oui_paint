#include "effects/curves.h"

#include <gtest/gtest.h>

using namespace oui_paint;

TEST(Curves, AStraightLineLeavesEveryLevelAsItIs)
{
    const auto table{curve_table({{0, 0}, {1, 1}})};
    for(std::size_t level{}; level < table.size(); ++ level)
    {
        EXPECT_NEAR(table[level], level / 255.0, 1e-9) << level;
    }
}

TEST(Curves, ACurveGoesThroughItsPointsAndNeverTurnsBackBetweenThem)
{
    const std::vector<CurvePoint> points{{0, 0}, {64 / 255.0, 0.5}, {192 / 255.0, 0.6}, {1, 1}};
    const auto table{curve_table(points)};
    EXPECT_NEAR(table[64], 0.5, 1e-9);
    EXPECT_NEAR(table[192], 0.6, 1e-9);
    for(std::size_t level{1}; level < table.size(); ++ level)
    {
        EXPECT_GE(table[level], table[level - 1] - 1e-12) << "rising points, a rising curve, at " << level;
    }
    for(std::size_t level{64}; level <= 192; ++ level)
    {
        EXPECT_LE(table[level], 0.6 + 1e-9) << "no bump over the higher point between two close ones";
    }
    const auto past{curve_table({{0.2, 0.3}, {0.8, 0.9}})};
    EXPECT_DOUBLE_EQ(past[0], 0.3) << "flat before the first point";
    EXPECT_DOUBLE_EQ(past[255], 0.9) << "and after the last";
}

TEST(Curves, AnEffectsValuesKeepTheCurveToEditAndItsTableToApply)
{
    std::map<std::string, double> values;
    EXPECT_EQ(curve_points(values, "r"), (std::vector<CurvePoint>{{0, 0}, {1, 1}})) << "none yet: the straight line";
    const std::vector<CurvePoint> points{{0, 0.1}, {0.5, 0.7}, {1, 0.9}};
    put_curve(values, "r", points);
    EXPECT_EQ(curve_points(values, "r"), points);
    EXPECT_NEAR(values.at("r0"), 0.1, 1e-9);
    EXPECT_NEAR(values.at("r255"), 0.9, 1e-9);
    put_curve(values, "r", {{0, 0}, {1, 1}});
    EXPECT_EQ(values.count("rpx2"), 0u) << "a point fewer, its values gone";
    EXPECT_EQ(curve_points(values, "g").size(), 2u);
}

TEST(Curves, TheHistogramCountsWhatIsSelectedAsMuchAsItIs)
{
    WorkImage image{4, 1};
    const float colors[4][4]{{1, 0, 0, 1}, {0, 0, 0, 1}, {0.5f, 0.5f, 0.5f, 0.5f}, {1, 1, 1, 1}};
    for(int x{}; x < 4; ++ x)
    {
        std::copy(colors[x], colors[x] + 4, image.at(x, 0));
    }
    Selection selection{4, 1};
    selection.add_rectangle({0, 0, 3, 1}, SelectionMode::replace);
    const auto histogram{histogram_of(image, selection)};
    EXPECT_DOUBLE_EQ(histogram.red[255], 1.0 + 0.5) << "the red pixel and the half see-through white, half";
    EXPECT_DOUBLE_EQ(histogram.red[0], 1.0) << "the black one";
    EXPECT_DOUBLE_EQ(histogram.luminosity[76], 1.0) << "red's luminosity";
    EXPECT_DOUBLE_EQ(histogram.blue[255], 0.5) << "the white pixel past the selection is not counted";
}

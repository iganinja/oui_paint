#include "ui/levels_dialog.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace oui_paint;

TEST(Levels, TheGreyMarkerIsWhereTheLevelThatComesOutHalfwayGoesIn)
{
    EXPECT_NEAR(gamma_marker(0, 255, 1.0), 0.5, 1e-9) << "no gamma: halfway";
    EXPECT_NEAR(gamma_marker(100, 200, 1.0), 150 / 255.0, 1e-9);
    EXPECT_LT(gamma_marker(0, 255, 2.0), 0.5) << "a gamma over 1 lightens: grey comes from darker";
    for(const auto gamma : {0.5, 1.0, 2.2, 4.0})
    {
        EXPECT_NEAR(gamma_at(30, 220, gamma_marker(30, 220, gamma)), gamma, 1e-6) << "the marker gives back its gamma";
    }
    // As the Levels plugin works it out: the level at the marker comes out halfway.
    const auto low{30.0};
    const auto high{220.0};
    const auto gamma{2.2};
    const auto stretched{(gamma_marker(low, high, gamma) * 255.0 - low) / (high - low)};
    EXPECT_NEAR(std::pow(stretched, 1.0 / gamma), 0.5, 1e-9);
}

TEST(Levels, AutoStretchesFromTheDarkestToTheLightestLevelThere)
{
    std::array<double, 256> histogram{};
    histogram[40] = 1000;
    histogram[200] = 1000;
    histogram[2] = 1;
    EXPECT_EQ(auto_levels(histogram), (std::pair<int, int>{40, 200})) << "a stray dark pixel does not count";
    EXPECT_EQ(auto_levels({}), (std::pair<int, int>{0, 255})) << "nothing there: as it is";
}

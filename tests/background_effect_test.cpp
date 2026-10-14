#include "app/background_effect.h"

#include <oui/scripted_backend.h>

#include <gtest/gtest.h>

#include <chrono>
#include <vector>

using namespace oui_paint;
using namespace olagarro;

namespace
{

WorkImage gray(int width, int height, float value)
{
    WorkImage image{width, height};
    for(std::size_t at{}; at < image.pixels.size(); ++ at)
    {
        image.pixels[at] = at % 4 == 3 ? 1.0f : value;
    }
    return image;
}

// Runs the application's frames until `done` says so, or a few seconds pass.
template <typename Done> bool run_until(oui::Application& application, Done done)
{
    const auto until{std::chrono::steady_clock::now() + std::chrono::seconds{10}};
    while(!done() && std::chrono::steady_clock::now() < until)
    {
        application.run_frame();
    }
    return done();
}

} // namespace

TEST(BackgroundEffect, GivesWhatTheLatestValuesMakeAndSkipsWhatWasAskedForInBetween)
{
    oui::Application application{oui::scripted_backend()};
    PluginHost host;
    host.load(R"(
        paint.effect({
            name: "Scale", parameters: [{id: "by", value: 1}],
            render(image, values) { return image.gaussianBlur(3).lookup([0, values.by]); }
        });)",
              "scale.js");
    const auto source{gray(200, 200, 1.0f)};
    std::vector<double> shown;
    float last_pixel{-1.0f};
    BackgroundEffect background{application, host, host.effects().at(0), source, {},
                                [&](const BackgroundEffect::Values& values, const WorkImage& result)
                                {
                                    shown.push_back(values.at("by"));
                                    last_pixel = result.at(100, 100)[0];
                                },
                                [](const std::string& problem) { FAIL() << problem; }};
    for(int step{1}; step <= 20; ++ step)
    {
        background.request({{"by", step / 20.0}});
    }
    ASSERT_TRUE(run_until(application, [&] { return !shown.empty() && shown.back() == 1.0; }));
    EXPECT_LT(shown.size(), 20u) << "the values asked for while one was being made are skipped";
    EXPECT_NEAR(last_pixel, 1.0f, 1e-4f);
    const auto last{background.stop()};
    ASSERT_TRUE(last);
    EXPECT_EQ(last->first.at("by"), 1.0);
}

TEST(BackgroundEffect, SaysWhatWentWrongAndMakesNothingMore)
{
    oui::Application application{oui::scripted_backend()};
    PluginHost host;
    host.load(R"(
        paint.effect({
            name: "Fails", parameters: [{id: "at", value: 0}],
            render(image, values) { if(values.at > 0) throw new Error("too far"); return image; }
        });)",
              "fails.js");
    const auto source{gray(8, 8, 0.5f)};
    std::string problem;
    auto made{0};
    BackgroundEffect background{application, host, host.effects().at(0), source, {},
                                [&](const BackgroundEffect::Values&, const WorkImage&) { ++ made; },
                                [&](const std::string& said) { problem = said; }};
    background.request({{"at", 1}});
    ASSERT_TRUE(run_until(application, [&] { return !problem.empty(); }));
    EXPECT_NE(problem.find("too far"), std::string::npos);
    background.request({{"at", 0}});
    EXPECT_FALSE(background.stop()) << "nothing made after it failed";
    EXPECT_EQ(made, 0);
}

#include "effects/plugins.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

WorkImage gray(int width, int height, float value)
{
    WorkImage image{width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto* const pixel{image.at(x, y)};
            pixel[0] = pixel[1] = pixel[2] = value;
            pixel[3] = 1.0f;
        }
    }
    return image;
}

const Effect& named(const PluginHost& host, std::string_view name)
{
    for(const auto& effect : host.effects())
    {
        if(effect.name == name)
        {
            return effect;
        }
    }
    throw std::runtime_error{"no effect called " + std::string{name}};
}

} // namespace

TEST(Plugins, APluginDeclaresAnEffectThatChainsPieces)
{
    PluginHost host;
    host.load(R"(
        paint.effect({
            name: "Darker Blur", menu: "Test",
            parameters: [{id: "radius", name: "Radius", min: 0, max: 10, value: 1}],
            render(image, values) { return image.gaussianBlur(values.radius).lookup([0, 0.5]); }
        });)",
              "test.js");
    const auto& effect{named(host, "Darker Blur")};
    ASSERT_EQ(effect.parameters.size(), 1u);
    EXPECT_EQ(effect.parameters[0].maximum, 10);
    const auto result{host.apply(effect, gray(8, 8, 1.0f), {{"radius", 2.0}}, {})};
    EXPECT_NEAR(result.at(4, 4)[0], 0.5, 1e-5);
}

TEST(Plugins, APluginCanWorkPixelByPixelInJavaScript)
{
    PluginHost host;
    host.load(R"(
        paint.effect({
            name: "Halve", parameters: [],
            render(image) {
                return image.rows((row, y) => { for(let i = 0; i < row.length; i++) row[i] *= 0.5; });
            }
        });)",
              "test.js");
    const auto result{host.apply(named(host, "Halve"), gray(3, 2, 0.8f), {}, {})};
    EXPECT_NEAR(result.at(2, 1)[0], 0.4, 1e-6);
    EXPECT_NEAR(result.at(2, 1)[3], 0.5, 1e-6);
}

TEST(Plugins, TheBuiltInEffectsLoadAndRun)
{
    PluginHost host;
    for(const auto& [source, name] : built_in_plugins())
    {
        host.load(source, name);
    }
    EXPECT_GE(host.effects().size(), 18u);
    EffectContext context;
    context.primary = {0, 0, 0, 1};
    context.secondary = {1, 1, 1, 1};
    for(const auto& effect : host.effects())
    {
        std::map<std::string, double> values;
        for(const auto& parameter : effect.parameters)
        {
            values[parameter.id] = parameter.value;
        }
        const auto result{host.apply(effect, gray(16, 16, 0.5f), values, context)};
        EXPECT_EQ(result.width, 16) << effect.name;
    }
    const auto inverted{host.apply(named(host, "Invert Colors"), gray(2, 2, 0.25f), {}, context)};
    EXPECT_NEAR(inverted.at(0, 0)[0], 0.75, 1e-5);
}

TEST(Plugins, HooksHearOfASave)
{
    PluginHost host;
    host.load(R"(paint.onSaved((path) => { if(path !== "/tmp/x.png") throw new Error("wrong path " + path); });)",
              "hook.js");
    EXPECT_NO_THROW(host.saved("/tmp/x.png"));
    EXPECT_THROW(host.saved("/tmp/y.png"), olagarro::oui::script::ScriptError);
}

TEST(Plugins, AParallelEffectRunsItsRowsOnEveryCoreAndGivesTheSameAsOne)
{
    PluginHost host;
    host.load(R"(
        paint.effect({
            name: "Ramp", parallel: true, parameters: [{id: "scale", value: 1}],
            row(row, y, values) {
                for(let i = 0; i < row.length; i += 4) { row[i] = (y % 7) / 7 * values.scale; row[i + 1] = i / row.length; }
            }
        });)",
              "parallel.js");
    const auto& effect{named(host, "Ramp")};
    EXPECT_TRUE(effect.parallel);
    const auto result{host.apply(effect, gray(64, 300, 0.5f), {{"scale", 1.0}}, {})};
    for(int y{}; y < 300; ++ y)
    {
        ASSERT_NEAR(result.at(10, y)[0], (y % 7) / 7.0, 1e-6) << "row " << y;
    }
    EXPECT_NEAR(result.at(32, 150)[1], 0.5, 1e-6);
}

TEST(Plugins, LevelsCanGiveEachColourLevelsOfItsOwn)
{
    PluginHost host;
    for(const auto& [source, name] : built_in_plugins())
    {
        host.load(source, name);
    }
    const std::map<std::string, double> values{{"inputLow", 0},  {"inputHigh", 255},  {"gamma", 1},
                                               {"outputLow", 0}, {"outputHigh", 255}, {"inputLow_r", 128}};
    const auto result{host.apply(named(host, "Levels"), gray(2, 2, 160.0f / 255.0f), values, {})};
    EXPECT_NEAR(result.at(0, 0)[0], 0.25f, 0.01f) << "red stretched from 128 up: 160 comes out at a quarter";
    EXPECT_NEAR(result.at(0, 0)[1], 160.0f / 255.0f, 1e-3f) << "green as it was";
    EXPECT_NEAR(result.at(0, 0)[2], 160.0f / 255.0f, 1e-3f);
}

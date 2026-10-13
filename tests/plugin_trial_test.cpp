#include "effects/plugin_trial.h"
#include "effects/test_card.h"

#include <gtest/gtest.h>

using namespace oui_paint;
using namespace std::chrono_literals;

namespace
{

Image white(int width, int height)
{
    Image image{PixelFormat::rgba8, width, height};
    std::fill(image.bytes.begin(), image.bytes.end(), std::uint8_t{255});
    return image;
}

PluginTrial trial(std::string_view source, std::string_view effect = {}, std::map<std::string, double> values = {})
{
    return try_plugin(source, "mine.js", effect, white(8, 4), values, {}, 2s);
}

} // namespace

TEST(PluginTrial, ShowsWhatTheEffectMakesAndWhatItPrinted)
{
    const auto tried{trial(R"(
        paint.effect({
            name: "Darken", menu: "Test",
            parameters: [{id: "level", name: "Level", min: 0, max: 1, step: 0, value: 0.5}],
            render(image, values) { console.log("level", values.level); return image.lookup([0, values.level]); }
        });)")};
    ASSERT_FALSE(tried.error) << *tried.error;
    ASSERT_EQ(tried.effects.size(), 1u);
    ASSERT_TRUE(tried.made);
    EXPECT_EQ(tried.made->pixel<Rgba8>(3, 2).value[0], 128);
    EXPECT_EQ(tried.log, "level 0.5\n");
}

TEST(PluginTrial, ValuesGivenAreUsedWithinRangeAndTheRestKeepTheirDefaults)
{
    const auto* const source{R"(
        paint.effect({name: "First", menu: "Test", render(image) { return image; }});
        paint.effect({
            name: "Second", menu: "Test",
            parameters: [{id: "a", name: "A", min: 0, max: 10, value: 3}, {id: "b", name: "B", min: 0, max: 10, value: 4}],
            render(image, values) { console.log(values.a, values.b); return image; }
        });)"};
    const auto tried{trial(source, "Second", {{"a", 99.0}})};
    ASSERT_EQ(tried.tried, std::optional<std::size_t>{1});
    EXPECT_EQ(tried.log, "10 4\n") << "a past its range is its maximum; b, not given, its default";
    EXPECT_EQ(trial(source, "Gone").tried, std::optional<std::size_t>{0}) << "a name it no longer has: the first";
}

TEST(PluginTrial, AFailureSaysItsLine)
{
    const auto broken{trial("paint.effect({\n  name: \"Broken\",\n  menu: \"Test\",,\n});\n")};
    ASSERT_TRUE(broken.error);
    EXPECT_EQ(broken.error_line, std::optional<std::size_t>{3}) << *broken.error;
    EXPECT_TRUE(broken.effects.empty());
    const auto throwing{trial("paint.effect({\n  name: \"Throws\", menu: \"Test\",\n  render(image) {\n"
                              "    return image.nothingHere();\n  },\n});\n")};
    ASSERT_TRUE(throwing.error);
    EXPECT_EQ(throwing.error_line, std::optional<std::size_t>{4}) << *throwing.error;
    EXPECT_EQ(throwing.effects.size(), 1u) << "it declared its effect before failing in it";
}

TEST(PluginTrial, APluginThatNeverEndsIsStopped)
{
    const auto start{std::chrono::steady_clock::now()};
    const auto tried{try_plugin(R"(paint.effect({name: "Forever", menu: "Test", render(image) { for(;;) {} }});)",
                                "mine.js", {}, white(4, 4), {}, {}, 200ms)};
    EXPECT_TRUE(tried.error);
    EXPECT_LT(std::chrono::steady_clock::now() - start, 5s);
}

TEST(PluginTrial, RowsMadeOnEveryCorePrintToo)
{
    const auto tried{try_plugin(R"(
        paint.effect({
            name: "Rows", menu: "Test", parallel: true,
            row(row, y) { if (y === 0) console.log("first row"); for (let i = 0; i < row.length; i += 4) row[i] = 0; }
        });)",
                                "mine.js", {}, white(4, 64), {}, {}, 2s)};
    ASSERT_FALSE(tried.error) << *tried.error;
    EXPECT_EQ(tried.made->pixel<Rgba8>(0, 63).value[0], 0);
    EXPECT_EQ(tried.log, "first row\n");
}

TEST(PluginTrial, ANewPluginWorksOnTheTestCardAsItStarts)
{
    const auto card{test_card()};
    const auto tried{try_plugin(new_plugin_source(), "new.js", {}, card, {}, {}, 5s)};
    ASSERT_FALSE(tried.error) << *tried.error;
    ASSERT_TRUE(tried.made);
    EXPECT_NE(tried.made->pixel<Rgba8>(2, 300).value, card.pixel<Rgba8>(2, 300).value) << "the lines softened";
}

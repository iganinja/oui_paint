#include "app/help.h"
#include "app/plugin_api.h"
#include "effects/plugin_trial.h"
#include "effects/plugins.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

using namespace oui_paint;
using olagarro::oui::suggestions_for;
using olagarro::oui::writing_at;

namespace
{

std::vector<std::string> labels(const std::vector<olagarro::oui::Suggestion>& suggestions)
{
    std::vector<std::string> found;
    for(const auto& suggestion : suggestions)
    {
        found.push_back(suggestion.label);
    }
    return found;
}

bool has(const std::vector<std::string>& labels, std::string_view label)
{
    return std::find(labels.begin(), labels.end(), label) != labels.end();
}

} // namespace

TEST(PluginApi, DeclaresEveryMethodAnImageHasInThePrelude)
{
    // The methods the prelude gives an image, read from its class, against those paint.d.ts declares.
    PluginHost host;
    std::set<std::string> declared;
    for(const auto& member : plugin_api().instance_members("paint.Image"))
    {
        declared.insert(member.label);
    }
    host.engine().run(R"(
        globalThis.__methods = Object.getOwnPropertyNames(Object.getPrototypeOf(paint.clouds(1, 1, 1, 0, 1)))
            .filter((name) => name !== "constructor").join(","); )",
                      "probe.js");
    host.load("paint.effect({name: \"Probe\", menu: \"Test\", parameters: [{id: \"m\", name: __methods, min: 0, max: 1, "
              "value: 0}], render(image) { return image; }});",
              "probe.js");
    const auto& methods{host.effects().back().parameters.front().name};
    std::set<std::string> offered;
    for(std::size_t start{}; start <= methods.size();)
    {
        const auto comma{std::min(methods.find(',', start), methods.size())};
        offered.insert(methods.substr(start, comma - start));
        start = comma + 1;
    }
    EXPECT_EQ(declared, offered);
}

TEST(PluginApi, InsideRenderTheImageSuggestsItsMethodsAndWhatTheyGiveDoesToo)
{
    const std::string code{new_plugin_source()};
    const auto at{code.find("const soft")};
    ASSERT_NE(at, std::string::npos);
    const std::string written{code.substr(0, at) + "image.gau"};
    const auto found{labels(suggestions_for(writing_at(written, written.size()), {written}, &javascript_words(),
                                            &plugin_api()))};
    EXPECT_TRUE(has(found, "gaussianBlur")) << "image, render's first parameter, is an image";
    const std::string chained{code.substr(0, code.find("return soft")) + "soft."};
    EXPECT_TRUE(has(labels(suggestions_for(writing_at(chained, chained.size()), {chained}, &javascript_words(),
                                           &plugin_api())),
                    "over"))
        << "soft is what gaussianBlur gives: an image";
    const auto top{labels(suggestions_for(writing_at("pai", 3), {std::string_view{"pai"}}, &javascript_words(),
                                          &plugin_api()))};
    EXPECT_TRUE(has(top, "paint"));
    const auto under{labels(suggestions_for(writing_at("paint.", 6), {std::string_view{"paint."}}, nullptr,
                                            &plugin_api()))};
    EXPECT_TRUE(has(under, "effect") && has(under, "clouds") && has(under, "onSaved"));
}

TEST(PluginApi, TheHelpListsTheImagesMethodsFromTheDeclarations)
{
    const auto pages{help_pages(olagarro::oui::CommandRegistry{})};
    const auto& plugins{pages.at(2).markdown};
    EXPECT_EQ(plugins.find("image-methods"), std::string::npos) << "the marks are gone";
    EXPECT_NE(plugins.find("| `gaussianBlur(radius: number): Image` | Blurred as by a lens"), std::string::npos)
        << plugins;
    EXPECT_NE(plugins.find("| `width: number` | Its width in pixels. |"), std::string::npos);
}

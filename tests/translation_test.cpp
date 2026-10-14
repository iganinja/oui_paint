#include "app/translations.h"
#include "effects/plugins.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace oui_paint;
namespace oui = olagarro::oui;

namespace oui_paint
{
std::vector<std::string_view> embedded_catalogs();
}

namespace
{

// Catalogs are kept by hand, so this stands in for a tool, as OUI's own check does: every text the sources pass to
// translate(), and every text of the built-in plugins, must be in every catalog, translated, unless OUI's catalog of
// that language has it; and a catalog has nothing else.

struct SourceText
{
    std::string context;
    std::string text;
    std::string where;
};

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// The source with comments blanked out, string literals kept.
std::string without_comments(const std::string& source)
{
    std::string result{source};
    for(std::size_t i{}; i < result.size(); ++ i)
    {
        if(result[i] == '"' || result[i] == '\'')
        {
            const auto quote{result[i]};
            for(++ i; i < result.size() && result[i] != quote; ++ i)
            {
                if(result[i] == '\\')
                {
                    ++ i;
                }
            }
        }
        else if(result.compare(i, 2, "//") == 0)
        {
            for(; i < result.size() && result[i] != '\n'; ++ i)
            {
                result[i] = ' ';
            }
        }
    }
    return result;
}

// Adjacent literals as one text, their escapes undone.
std::string unescape_literals(const std::string& literals)
{
    std::string result;
    auto inside{false};
    for(std::size_t i{}; i < literals.size(); ++ i)
    {
        const auto c{literals[i]};
        if(c == '"')
        {
            inside = !inside;
        }
        else if(inside && c == '\\' && i + 1 < literals.size())
        {
            const auto escaped{literals[++ i]};
            result += escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped;
        }
        else if(inside)
        {
            result += c;
        }
    }
    return result;
}

std::vector<SourceText> source_texts(const std::filesystem::path& folder, std::vector<std::string>& problems)
{
    static const std::regex call{R"re(\btranslate\s*\()re"};
    static const std::regex literal_call{
        R"re(^translate\s*\(\s*"((?:[^"\\]|\\.)*)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\))re"};
    std::vector<SourceText> texts;
    for(const auto& entry : std::filesystem::recursive_directory_iterator{folder})
    {
        const auto extension{entry.path().extension()};
        // translations.cpp passes the plugins' texts on, which come from the plugins themselves, below.
        if((extension != ".h" && extension != ".cpp") || entry.path().stem() == "translations")
        {
            continue;
        }
        const auto source{without_comments(read_file(entry.path()))};
        for(auto found{std::sregex_iterator{source.begin(), source.end(), call}}; found != std::sregex_iterator{};
            ++ found)
        {
            const auto offset{static_cast<std::size_t>(found->position())};
            const auto line{1 + std::count(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(offset), '\n')};
            const auto where{entry.path().filename().string() + ":" + std::to_string(line)};
            std::smatch match;
            const std::string rest{source.substr(offset, 2000)};
            if(!std::regex_search(rest, match, literal_call))
            {
                problems.push_back(where + ": translate() without string literals");
                continue;
            }
            texts.push_back({unescape_literals("\"" + match[1].str() + "\""), unescape_literals(match[2].str()), where});
        }
    }
    return texts;
}

// What the built-in plugins show: their effects' names and menus, their parameters' names and options, and the
// indexers' and converters'.
std::vector<SourceText> plugin_texts()
{
    PluginHost host;
    for(const auto& [source, name] : built_in_plugins())
    {
        host.load(source, name);
    }
    std::vector<SourceText> texts;
    const auto add{[&texts](const std::string& text, const std::string& where)
                   {
                       if(!text.empty())
                       {
                           texts.push_back({"Plugins", text, where});
                       }
                   }};
    const auto add_parameters{[&add](const std::vector<EffectParameter>& parameters, const std::string& where)
                              {
                                  for(const auto& parameter : parameters)
                                  {
                                      add(parameter.name, where);
                                      for(const auto& option : parameter.options)
                                      {
                                          add(option, where);
                                      }
                                  }
                              }};
    for(const auto& effect : host.effects())
    {
        add(effect.name, "effect " + effect.name);
        add(effect.menu, "effect " + effect.name);
        add_parameters(effect.parameters, "effect " + effect.name);
    }
    for(const auto& indexer : host.indexers())
    {
        add(indexer.name, "indexer " + indexer.name);
        add_parameters(indexer.parameters, "indexer " + indexer.name);
    }
    for(const auto& converter : host.converters())
    {
        add(converter.name, "converter " + converter.name);
        add_parameters(converter.parameters, "converter " + converter.name);
    }
    return texts;
}

std::set<std::string> placeholders(const std::string& text)
{
    static const std::regex placeholder{R"(\{[0-9]+\})"};
    std::set<std::string> result;
    for(auto found{std::sregex_iterator{text.begin(), text.end(), placeholder}}; found != std::sregex_iterator{};
        ++ found)
    {
        result.insert(found->str());
    }
    return result;
}

// Whether the text marks a letter with &, && being an & shown.
bool has_mnemonic(const std::string& text)
{
    for(std::size_t i{}; i + 1 < text.size(); ++ i)
    {
        if(text[i] == '&')
        {
            if(text[i + 1] != '&')
            {
                return true;
            }
            ++ i;
        }
    }
    return false;
}

} // namespace

TEST(Translations, TheCatalogsCoverEveryTextAndNothingElse)
{
    const std::filesystem::path sources{OUI_PAINT_SOURCE_DIR "/src"};
    if(!std::filesystem::is_directory(sources))
    {
        GTEST_SKIP() << "the sources are not here, as on another machine";
    }
    std::vector<std::string> problems;
    auto texts{source_texts(sources, problems)};
    const auto plugins{plugin_texts()};
    texts.insert(texts.end(), plugins.begin(), plugins.end());
    ASSERT_FALSE(texts.empty());
    for(const auto& catalog_text : embedded_catalogs())
    {
        std::string error;
        const auto catalog{oui::parse_translation_catalog(catalog_text, error)};
        ASSERT_TRUE(catalog) << error;
        const auto file{catalog->language + ".json"};
        EXPECT_FALSE(catalog->name.empty()) << file;
        const oui::Translation* library{};
        for(const auto& translation : oui::oui_translations())
        {
            if(translation.language() == catalog->language)
            {
                library = &translation;
            }
        }
        std::set<std::pair<std::string, std::string>> used;
        for(const auto& text : texts)
        {
            used.insert({text.context, text.text});
            const auto quoted{text.context + " \"" + text.text + "\" (" + text.where + ")"};
            const std::string* translated{};
            if(const auto context{catalog->contexts.find(text.context)}; context != catalog->contexts.end())
            {
                if(const auto found{context->second.find(text.text)}; found != context->second.end() && !found->second.empty())
                {
                    translated = &found->second;
                }
            }
            if(translated == nullptr && library != nullptr)
            {
                translated = library->find(text.context, text.text);
            }
            if(translated == nullptr)
            {
                problems.push_back(file + ": untranslated " + quoted);
                continue;
            }
            if(placeholders(*translated) != placeholders(text.text))
            {
                problems.push_back(file + ": other placeholders in " + quoted);
            }
            if(has_mnemonic(*translated) != has_mnemonic(text.text))
            {
                problems.push_back(file + ": the '&' mnemonic comes and goes in " + quoted);
            }
        }
        for(const auto& [context, entries] : catalog->contexts)
        {
            for(const auto& [text, translation] : entries)
            {
                if(used.count({context, text}) == 0)
                {
                    problems.push_back(file + ": no source uses " + context + " \"" + text + "\"");
                }
            }
        }
    }
    std::string report;
    for(const auto& problem : problems)
    {
        report += problem + "\n";
    }
    EXPECT_TRUE(problems.empty()) << problems.size() << " problems:\n" << report;
}

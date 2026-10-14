#pragma once

#include "core/color.h"
#include "effects/pieces.h"
#include "tools/stroke.h"

#include <oui/script/script_engine.h>

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A number an effect asks for, as its dialog shows it: a slider and a field, or a choice when it has options.
struct EffectParameter
{
    std::string id;
    std::string name;
    double minimum{0.0};
    double maximum{100.0};
    double value{0.0};
    // What every value is a whole number of; 0 for any.
    double step{1.0};
    // Names to choose from; the value is the index of the one chosen.
    std::vector<std::string> options;
};

// An effect a plugin declared.
struct Effect
{
    std::string name;
    // "Adjustments", or the Effects submenu it goes in, as "Blurs".
    std::string menu;
    std::vector<EffectParameter> parameters;
    std::size_t index{0};
    // Made row by row by its `row` function rather than by `render`.
    bool by_rows{false};
    // Its rows may be made at once on every core, each core running its own copy of the plugins: `row` reads only the
    // row it is given and writes only it.
    bool parallel{false};
    // The program's own dialog for it, in place of a slider for each parameter: "curves" or "levels".
    std::string dialog;
};

// What an effect is told besides its parameters' values.
struct EffectContext
{
    Color primary;
    Color secondary;
    // The selection's bounds, the whole image when nothing is selected.
    int selection_x{0};
    int selection_y{0};
    int selection_width{0};
    int selection_height{0};
};

// A way of turning an image into indices of a palette, as a plugin declares it with paint.indexer: its parameters,
// and whether it says itself which index each pixel takes, or leaves it to the nearest colour.
struct Indexer
{
    std::string name;
    std::vector<EffectParameter> parameters;
    std::size_t index{0};
    bool assigns{false};
};

// What an indexer is told: how many colours its palette may have, and the palette to use when one is given, as when an
// image goes into a palette loaded.
struct IndexContext
{
    int count{256};
    std::optional<std::vector<Color>> palette;
};

// What an indexer made: its palette, and an image of indices, as indexing.h describes them.
struct Indexed
{
    std::vector<Color> palette;
    WorkImage indices;
};

// A way of changing an image's format, as a plugin declares it with paint.converter: "grayscale", from colour to
// grey, or "8 bits", from 16 bits a channel to 8, and its parameters.
struct Converter
{
    std::string name;
    std::string to;
    std::vector<EffectParameter> parameters;
    std::size_t index{0};
};

// Runs plugins: JavaScript that declares effects with `paint.effect`, and hooks with `paint.onSaved`, building on the
// pieces in C++.
class PluginHost
{
  public:
    PluginHost();
    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Runs a plugin's source; its effects are added. Throws ScriptError when it fails.
    void load(std::string_view source, std::string_view name);
    [[nodiscard]] const std::vector<Effect>& effects() const { return m_effects; }
    [[nodiscard]] const std::vector<Indexer>& indexers() const { return m_indexers; }
    [[nodiscard]] const std::vector<Converter>& converters() const { return m_converters; }
    // What an indexer makes of `image`, with these values of its parameters. Throws ScriptError when it fails, or
    // gives a palette of more colours than it may, or indices past it.
    [[nodiscard]] Indexed index(const Indexer& indexer, const WorkImage& image,
                                const std::map<std::string, double>& values, const IndexContext& context);
    // What a converter makes of `image`, its colours as the format it converts to will keep them. Throws ScriptError
    // when it fails.
    [[nodiscard]] WorkImage convert(const Converter& converter, const WorkImage& image,
                                    const std::map<std::string, double>& values);
    // An effect's result on `image` with these values of its parameters, by id. Throws ScriptError when it fails.
    [[nodiscard]] WorkImage apply(const Effect& effect, const WorkImage& image,
                                  const std::map<std::string, double>& values, const EffectContext& context);
    // Tells the hooks a file was saved.
    void saved(std::string_view path);
    // How long loading a plugin or making an effect may take, two minutes unless set, and where console.log writes,
    // nowhere unless set: also for the copies that make rows on other cores, so `print` may be called from them.
    void set_time_limit(std::chrono::milliseconds limit);
    void set_print(std::function<void(std::string_view text)> print);
    [[nodiscard]] olagarro::oui::script::ScriptEngine& engine() { return m_engine; }

  private:
    // Runs a row effect's `row` on every row of `band`, whose first row is `first_row` of the image.
    [[nodiscard]] WorkImage apply_rows(const Effect& effect, const WorkImage& band, int first_row,
                                       const std::string& values, const EffectContext& context);
    [[nodiscard]] int keep(WorkImage image);
    [[nodiscard]] const WorkImage& image(const olagarro::oui::script::Value& id) const;

    olagarro::oui::script::ScriptEngine m_engine;
    std::vector<Effect> m_effects;
    std::vector<Indexer> m_indexers;
    std::vector<Converter> m_converters;
    olagarro::oui::script::Value m_index_runner;
    olagarro::oui::script::Value m_convert_runner;
    // The images an effect works on while it runs, by the ids scripts hold.
    std::map<int, WorkImage> m_images;
    // The image an effect is given, lent rather than copied, as big as images are: read, never written.
    const WorkImage* m_lent{};
    int m_lent_id{0};
    int m_next_id{1};
    olagarro::oui::script::Value m_runner;
    olagarro::oui::script::Value m_row_runner;
    // Every plugin loaded, in order, for the copies that make rows on other cores.
    std::vector<std::pair<std::string, std::string>> m_sources;
    olagarro::oui::script::Value m_saved;
    std::chrono::milliseconds m_time_limit{std::chrono::minutes{2}};
    std::function<void(std::string_view)> m_print;
};

// The plugins built into OUI Paint, their sources and names.
[[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>> built_in_plugins();

} // namespace oui_paint

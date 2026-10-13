#pragma once

#include "effects/pieces.h"
#include "tools/stroke.h"

#include <oui/script/script_engine.h>

#include <map>
#include <memory>
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
    // An effect's result on `image` with these values of its parameters, by id. Throws ScriptError when it fails.
    [[nodiscard]] WorkImage apply(const Effect& effect, const WorkImage& image,
                                  const std::map<std::string, double>& values, const EffectContext& context);
    // Tells the hooks a file was saved.
    void saved(std::string_view path);
    [[nodiscard]] olagarro::oui::script::ScriptEngine& engine() { return m_engine; }

  private:
    // Runs a row effect's `row` on every row of `band`, whose first row is `first_row` of the image.
    [[nodiscard]] WorkImage apply_rows(const Effect& effect, const WorkImage& band, int first_row,
                                       const std::string& values, const EffectContext& context);
    [[nodiscard]] int keep(WorkImage image);
    [[nodiscard]] const WorkImage& image(const olagarro::oui::script::Value& id) const;

    olagarro::oui::script::ScriptEngine m_engine;
    std::vector<Effect> m_effects;
    // The images an effect works on while it runs, by the ids scripts hold.
    std::map<int, WorkImage> m_images;
    int m_next_id{1};
    olagarro::oui::script::Value m_runner;
    olagarro::oui::script::Value m_row_runner;
    // Every plugin loaded, in order, for the copies that make rows on other cores.
    std::vector<std::pair<std::string, std::string>> m_sources;
    olagarro::oui::script::Value m_saved;
};

// The plugins built into OUI Paint, their sources and names.
[[nodiscard]] std::vector<std::pair<std::string_view, std::string_view>> built_in_plugins();

} // namespace oui_paint

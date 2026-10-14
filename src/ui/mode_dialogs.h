#pragma once

#include "core/color.h"
#include "effects/plugins.h"

#include <oui/application.h>

#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace oui_paint
{

// The ways a plugin offers of doing one thing, as indexers or converters, which one is chosen and the values of each
// one's parameters, kept from one time to the next.
struct MethodChoice
{
    std::vector<std::string> names;
    std::vector<std::vector<EffectParameter>> parameters;
    std::size_t chosen{0};
    // By method's name, so they stay as plugins are loaded again.
    std::map<std::string, std::map<std::string, double>> values;

    // The methods offered now, the one chosen by name kept chosen when it still is.
    void offer(std::vector<std::string> new_names, std::vector<std::vector<EffectParameter>> new_parameters);
    // The chosen method's values, its parameters' starting ones where not set.
    [[nodiscard]] std::map<std::string, double> chosen_values();
};

// What Image > Mode > Indexed asks: how, into how many colours, and whether into a palette loaded.
struct IndexingChoice
{
    MethodChoice method;
    int count{256};
    // A palette loaded, by its file's name, and whether the image goes into it rather than one made.
    std::optional<std::pair<std::string, std::vector<Color>>> loaded;
    bool into_loaded{false};
};

// Asks how to make the image indexed. `load` asks for a palette file, none when given up; `changed` makes the image
// show the choice, once a frame however many changes come, and gives what went wrong, if anything, which the dialog
// shows. True when accepted.
[[nodiscard]] bool ask_indexing(olagarro::oui::Application& application, IndexingChoice& choice,
                                const std::function<std::optional<std::pair<std::string, std::vector<Color>>>()>& load,
                                const std::function<std::string()>& changed);

// Asks how to change the image's format, a method for each step, each under its title, as "Grey" and "8 bits";
// `changed` as ask_indexing's. True when accepted.
[[nodiscard]] bool ask_conversion(olagarro::oui::Application& application, const std::string& title,
                                  const std::vector<std::pair<std::string, MethodChoice*>>& steps,
                                  const std::function<std::string()>& changed);

} // namespace oui_paint

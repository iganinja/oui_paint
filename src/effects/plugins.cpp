#include "effects/plugins.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <thread>

using namespace olagarro;
using oui::script::ScriptError;
using oui::script::Value;

// Defined in the source CMake generates from the plugins built in.
std::vector<std::string_view> embedded_plugins();
std::vector<std::string_view> embedded_plugins_names();

namespace oui_paint
{

namespace
{

// A number as JSON writes it.
std::string json_number(double value)
{
    if(!std::isfinite(value))
    {
        return "0";
    }
    char text[40];
    std::snprintf(text, sizeof text, "%.17g", value);
    return text;
}

std::string json_color(const Color& color)
{
    return "[" + json_number(color.red) + "," + json_number(color.green) + "," + json_number(color.blue) + "," +
           json_number(color.alpha) + "]";
}

std::string json_string(std::string_view text)
{
    std::string result{"\""};
    for(const auto character : text)
    {
        if(character == '"' || character == '\\')
        {
            result += '\\';
        }
        result += character;
    }
    return result + "\"";
}

BlendMode mode_named(std::string_view name)
{
    for(const auto mode : all_blend_modes())
    {
        if(blend_mode_name(mode) == name)
        {
            return mode;
        }
    }
    throw ScriptError{"no blend mode is called " + std::string{name}};
}

std::vector<double> table_of(const Value& value)
{
    const auto floats{value.to_floats()};
    return {floats.begin(), floats.end()};
}

} // namespace

PluginHost::PluginHost()
{
    // Effects may take a while on big images; a plugin that loops forever is still stopped.
    m_engine.set_time_limit(m_time_limit);
    const auto number{[](const std::vector<Value>& arguments, std::size_t index)
                      { return index < arguments.size() ? arguments[index].to_number() : 0.0; }};
    const auto define_image{[this](std::string_view name, std::function<WorkImage(const std::vector<Value>&)> make)
                            {
                                m_engine.define(std::string{"__paint."} + std::string{name},
                                                [this, make](const std::vector<Value>& arguments)
                                                { return m_engine.number(keep(make(arguments))); });
                            }};
    m_engine.define("__paint.width", [this](const std::vector<Value>& arguments)
                    { return m_engine.number(image(arguments.at(0)).width); });
    m_engine.define("__paint.height", [this](const std::vector<Value>& arguments)
                    { return m_engine.number(image(arguments.at(0)).height); });
    define_image("copy", [this](const std::vector<Value>& arguments) { return image(arguments.at(0)); });
    define_image("gaussianBlur", [this, number](const std::vector<Value>& arguments)
                 { return gaussian_blur(image(arguments.at(0)), number(arguments, 1)); });
    define_image("boxBlur", [this, number](const std::vector<Value>& arguments)
                 { return box_blur(image(arguments.at(0)), static_cast<int>(number(arguments, 1))); });
    define_image("motionBlur", [this, number](const std::vector<Value>& arguments)
                 { return motion_blur(image(arguments.at(0)), number(arguments, 1), number(arguments, 2)); });
    define_image("radialBlur", [this, number](const std::vector<Value>& arguments) {
        return radial_blur(image(arguments.at(0)), number(arguments, 1), number(arguments, 2), number(arguments, 3));
    });
    define_image("zoomBlur", [this, number](const std::vector<Value>& arguments) {
        return zoom_blur(image(arguments.at(0)), number(arguments, 1), number(arguments, 2), number(arguments, 3));
    });
    define_image("convolve",
                 [this, number](const std::vector<Value>& arguments)
                 {
                     const auto matrix{table_of(arguments.at(1))};
                     const auto size{static_cast<int>(std::lround(std::sqrt(static_cast<double>(matrix.size()))))};
                     if(size * size != static_cast<int>(matrix.size()) || size % 2 == 0)
                     {
                         throw ScriptError{"a convolution matrix is square, of an odd size"};
                     }
                     return convolve(image(arguments.at(0)), matrix, size, number(arguments, 2), number(arguments, 3));
                 });
    define_image("lookup", [this](const std::vector<Value>& arguments) {
        return lookup(image(arguments.at(0)),
                      {table_of(arguments.at(1)), table_of(arguments.at(2)), table_of(arguments.at(3))});
    });
    define_image("lookupLuminosity", [this](const std::vector<Value>& arguments) {
        return lookup_luminosity(image(arguments.at(0)), table_of(arguments.at(1)));
    });
    define_image("hueSaturation", [this, number](const std::vector<Value>& arguments) {
        return hue_saturation(image(arguments.at(0)), number(arguments, 1), number(arguments, 2),
                              number(arguments, 3));
    });
    define_image("mix", [this, number](const std::vector<Value>& arguments) {
        return mix(image(arguments.at(0)), image(arguments.at(1)), mode_named(arguments.at(2).to_string()),
                   number(arguments, 3));
    });
    define_image("clouds",
                 [number](const std::vector<Value>& arguments)
                 {
                     const auto width{static_cast<int>(number(arguments, 0))};
                     const auto height{static_cast<int>(number(arguments, 1))};
                     if(width <= 0 || height <= 0)
                     {
                         throw ScriptError{"clouds need a size"};
                     }
                     return clouds(width, height, number(arguments, 2), number(arguments, 3),
                                   static_cast<std::uint32_t>(number(arguments, 4)));
                 });
    m_engine.define("__paint.row",
                    [this](const std::vector<Value>& arguments)
                    {
                        auto& work{m_images.at(static_cast<int>(arguments.at(0).to_number()))};
                        const auto y{static_cast<int>(arguments.at(1).to_number())};
                        if(y < 0 || y >= work.height)
                        {
                            throw ScriptError{"no such row"};
                        }
                        return m_engine.float_array(work.at(0, y), static_cast<std::size_t>(work.width) * 4u);
                    });
    m_engine.define("__paint.setRow",
                    [this](const std::vector<Value>& arguments)
                    {
                        auto& work{m_images.at(static_cast<int>(arguments.at(0).to_number()))};
                        const auto y{static_cast<int>(arguments.at(1).to_number())};
                        const auto row{arguments.at(2).to_floats()};
                        if(y < 0 || y >= work.height || row.size() != static_cast<std::size_t>(work.width) * 4u)
                        {
                            throw ScriptError{"a row is four numbers for each pixel of it"};
                        }
                        std::copy(row.begin(), row.end(), work.at(0, y));
                        return m_engine.undefined();
                    });
    m_engine.define("__paint.register",
                    [this](const std::vector<Value>& arguments)
                    {
                        const auto& spec{arguments.at(0)};
                        Effect effect;
                        effect.name = spec.get("name").to_string();
                        effect.menu = spec.get("menu").is_undefined() ? std::string{"Other"}
                                                                      : spec.get("menu").to_string();
                        effect.index = m_effects.size();
                        effect.by_rows = spec.get("row").is_function();
                        effect.parallel = effect.by_rows && spec.get("parallel").to_bool();
                        effect.dialog = spec.get("dialog").is_undefined() ? std::string{}
                                                                          : spec.get("dialog").to_string();
                        if(!effect.by_rows && !spec.get("render").is_function())
                        {
                            throw ScriptError{effect.name + " has neither render nor row"};
                        }
                        const auto parameters{spec.get("parameters")};
                        for(std::size_t index{}; index < parameters.length(); ++ index)
                        {
                            const auto parameter{parameters.at(index)};
                            EffectParameter made;
                            made.id = parameter.get("id").to_string();
                            made.name = parameter.get("name").is_undefined() ? made.id
                                                                             : parameter.get("name").to_string();
                            const auto options{parameter.get("options")};
                            for(std::size_t option{}; option < options.length(); ++ option)
                            {
                                made.options.push_back(options.at(option).to_string());
                            }
                            const auto read{[&parameter](std::string_view key, double fallback)
                                            {
                                                const auto value{parameter.get(key)};
                                                return value.is_undefined() ? fallback : value.to_number();
                                            }};
                            made.minimum = read("min", 0.0);
                            made.maximum = read("max", made.options.empty() ? 100.0 : made.options.size() - 1.0);
                            made.value = read("value", made.minimum);
                            made.step = read("step", 1.0);
                            effect.parameters.push_back(std::move(made));
                        }
                        m_effects.push_back(std::move(effect));
                        return m_engine.undefined();
                    });
    m_engine.define("__paint.setRunner", [this](const std::vector<Value>& arguments) {
        m_runner = arguments.at(0);
        return m_engine.undefined();
    });
    m_engine.define("__paint.setRowRunner", [this](const std::vector<Value>& arguments) {
        m_row_runner = arguments.at(0);
        return m_engine.undefined();
    });
    m_engine.define("__paint.setSaved", [this](const std::vector<Value>& arguments) {
        m_saved = arguments.at(0);
        return m_engine.undefined();
    });
    const auto sources{embedded_plugins()};
    const auto names{embedded_plugins_names()};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        if(names[index] == "prelude.js")
        {
            m_engine.run(sources[index], "prelude.js");
        }
    }
}

int PluginHost::keep(WorkImage made)
{
    const auto id{m_next_id++};
    m_images.emplace(id, std::move(made));
    return id;
}

const WorkImage& PluginHost::image(const Value& id) const
{
    const auto found{m_images.find(static_cast<int>(id.to_number()))};
    if(found == m_images.end())
    {
        throw ScriptError{"that image is gone"};
    }
    return found->second;
}

void PluginHost::set_time_limit(std::chrono::milliseconds limit)
{
    m_time_limit = limit;
    m_engine.set_time_limit(limit);
}

void PluginHost::set_print(std::function<void(std::string_view text)> print)
{
    m_print = print;
    m_engine.set_print(std::move(print));
}

void PluginHost::load(std::string_view source, std::string_view name)
{
    m_engine.run(source, name);
    m_sources.emplace_back(source, name);
}

WorkImage PluginHost::apply_rows(const Effect& effect, const WorkImage& band, int first_row, const std::string& values,
                                 const EffectContext& context)
{
    const auto context_json{"{\"firstRow\":" + json_number(first_row) + ",\"primary\":" + json_color(context.primary) +
                            ",\"secondary\":" + json_color(context.secondary) + "}"};
    m_images.clear();
    const auto id{keep(band)};
    static_cast<void>(m_row_runner.call({m_engine.number(static_cast<double>(effect.index)), m_engine.number(id),
                                         m_engine.string(values), m_engine.string(context_json)}));
    auto made{std::move(m_images.at(id))};
    m_images.clear();
    return made;
}

WorkImage PluginHost::apply(const Effect& effect, const WorkImage& source, const std::map<std::string, double>& values,
                            const EffectContext& context)
{
    std::string json{"{"};
    for(const auto& [id, value] : values)
    {
        json += (json.size() > 1 ? "," : "") + json_string(id) + ":" + json_number(value);
    }
    json += "}";
    const auto context_json{"{\"primary\":" + json_color(context.primary) + ",\"secondary\":" +
                            json_color(context.secondary) + ",\"selection\":{\"x\":" +
                            json_number(context.selection_x) + ",\"y\":" + json_number(context.selection_y) +
                            ",\"width\":" + json_number(context.selection_width) + ",\"height\":" +
                            json_number(context.selection_height) + "}}"};
    if(effect.by_rows)
    {
        const auto cores{std::max(1u, std::thread::hardware_concurrency())};
        const auto bands{effect.parallel ? std::min<int>(static_cast<int>(std::min(cores, 16u)), source.height) : 1};
        if(bands <= 1)
        {
            return apply_rows(effect, source, 0, json, context);
        }
        // Each band of rows on a thread of its own, with a copy of the plugins of its own, as an engine runs on the
        // thread that made it.
        WorkImage result{source.width, source.height};
        std::vector<std::thread> threads;
        std::vector<std::exception_ptr> failures(static_cast<std::size_t>(bands));
        for(int band{}; band < bands; ++ band)
        {
            const auto first{source.height * band / bands};
            const auto last{source.height * (band + 1) / bands};
            threads.emplace_back(
                [&, band, first, last]
                {
                    try
                    {
                        PluginHost worker;
                        worker.set_time_limit(m_time_limit);
                        if(m_print)
                        {
                            worker.set_print(m_print);
                        }
                        for(const auto& [code, name] : m_sources)
                        {
                            worker.load(code, name);
                        }
                        WorkImage rows{source.width, last - first};
                        std::copy(source.at(0, first), source.at(0, first) + rows.pixels.size(), rows.pixels.begin());
                        const auto made{worker.apply_rows(worker.effects().at(effect.index), rows, first, json,
                                                          context)};
                        std::copy(made.pixels.begin(), made.pixels.end(), result.at(0, first));
                    }
                    catch(...)
                    {
                        failures[static_cast<std::size_t>(band)] = std::current_exception();
                    }
                });
        }
        for(auto& thread : threads)
        {
            thread.join();
        }
        for(const auto& failure : failures)
        {
            if(failure)
            {
                std::rethrow_exception(failure);
            }
        }
        return result;
    }
    m_images.clear();
    const auto id{keep(source)};
    const auto result{m_runner.call({m_engine.number(static_cast<double>(effect.index)), m_engine.number(id),
                                     m_engine.string(json), m_engine.string(context_json)})};
    const auto found{m_images.find(static_cast<int>(result.to_number()))};
    if(found == m_images.end())
    {
        throw ScriptError{effect.name + " gave back no image"};
    }
    auto made{std::move(found->second)};
    m_images.clear();
    if(made.width != source.width || made.height != source.height)
    {
        throw ScriptError{effect.name + " gave back an image of another size"};
    }
    return made;
}

void PluginHost::saved(std::string_view path)
{
    if(m_saved.is_function())
    {
        static_cast<void>(m_saved.call({m_engine.string(path)}));
    }
}

std::vector<std::pair<std::string_view, std::string_view>> built_in_plugins()
{
    std::vector<std::pair<std::string_view, std::string_view>> plugins;
    const auto sources{embedded_plugins()};
    const auto names{embedded_plugins_names()};
    for(std::size_t index{}; index < names.size(); ++ index)
    {
        if(names[index] != "prelude.js")
        {
            plugins.emplace_back(sources[index], names[index]);
        }
    }
    return plugins;
}

} // namespace oui_paint

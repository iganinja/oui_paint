#include "effects/plugins.h"

#include "core/json.h"
#include "effects/indexing.h"

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

// The parameters a plugin declares, as a dialog shows them.
std::vector<EffectParameter> parameters_of(const Value& parameters)
{
    std::vector<EffectParameter> found;
    for(std::size_t index{}; index < parameters.length(); ++ index)
    {
        const auto parameter{parameters.at(index)};
        EffectParameter made;
        made.id = parameter.get("id").to_string();
        made.name = parameter.get("name").is_undefined() ? made.id : parameter.get("name").to_string();
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
        found.push_back(std::move(made));
    }
    return found;
}

// A palette as a plugin gives it, colours of three or four numbers from 0 to 1, or as the pieces take it, a flat
// list of three numbers a colour.
std::vector<Color> palette_of_numbers(const std::vector<double>& numbers)
{
    std::vector<Color> palette;
    for(std::size_t at{}; at + 2 < numbers.size(); at += 3)
    {
        palette.push_back({numbers[at], numbers[at + 1], numbers[at + 2], 1.0});
    }
    return palette;
}

std::string json_of_palette(const std::vector<Color>& palette)
{
    std::string json{"["};
    for(const auto& color : palette)
    {
        json += (json.size() > 1 ? "," : "") + std::string{"["} + json_number(color.red) + "," +
                json_number(color.green) + "," + json_number(color.blue) + "]";
    }
    return json + "]";
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
    define_image("addNoise", [this, number](const std::vector<Value>& arguments) {
        return add_noise(image(arguments.at(0)), number(arguments, 1), number(arguments, 2), number(arguments, 3),
                         static_cast<std::uint32_t>(number(arguments, 4)));
    });
    m_engine.define("__paint.row",
                    [this](const std::vector<Value>& arguments)
                    {
                        const auto& work{image(arguments.at(0))};
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
                        effect.parameters = parameters_of(spec.get("parameters"));
                        m_effects.push_back(std::move(effect));
                        return m_engine.undefined();
                    });
    // Indexers and converters, as plugins declare them.
    m_engine.define("__paint.registerIndexer",
                    [this](const std::vector<Value>& arguments)
                    {
                        const auto& spec{arguments.at(0)};
                        Indexer indexer;
                        indexer.name = spec.get("name").to_string();
                        if(!spec.get("palette").is_function())
                        {
                            throw ScriptError{indexer.name + " has no palette"};
                        }
                        indexer.assigns = spec.get("assign").is_function();
                        indexer.parameters = parameters_of(spec.get("parameters"));
                        indexer.index = m_indexers.size();
                        m_indexers.push_back(std::move(indexer));
                        return m_engine.undefined();
                    });
    m_engine.define("__paint.registerConverter",
                    [this](const std::vector<Value>& arguments)
                    {
                        const auto& spec{arguments.at(0)};
                        Converter converter;
                        converter.name = spec.get("name").to_string();
                        converter.to = spec.get("to").to_string();
                        if(converter.to != "grayscale" && converter.to != "8 bits")
                        {
                            throw ScriptError{converter.name + " converts to \"grayscale\" or \"8 bits\", not \"" +
                                              converter.to + "\""};
                        }
                        if(!spec.get("render").is_function())
                        {
                            throw ScriptError{converter.name + " has no render"};
                        }
                        converter.parameters = parameters_of(spec.get("parameters"));
                        converter.index = m_converters.size();
                        m_converters.push_back(std::move(converter));
                        return m_engine.undefined();
                    });
    m_engine.define("__paint.setIndexRunner", [this](const std::vector<Value>& arguments) {
        m_index_runner = arguments.at(0);
        return m_engine.undefined();
    });
    m_engine.define("__paint.setConvertRunner", [this](const std::vector<Value>& arguments) {
        m_convert_runner = arguments.at(0);
        return m_engine.undefined();
    });
    // The pieces that index, as indexing.h has them; palettes cross as flat lists of three numbers a colour.
    m_engine.define("__paint.colors",
                    [this, number](const std::vector<Value>& arguments)
                    {
                        std::string json{"["};
                        for(const auto& [color, count] :
                            distinct_colors(image(arguments.at(0)), static_cast<std::size_t>(number(arguments, 1))))
                        {
                            json += (json.size() > 1 ? "," : "") + std::string{"{\"color\":["} +
                                    json_number(color.red) + "," + json_number(color.green) + "," +
                                    json_number(color.blue) + "],\"count\":" + std::to_string(count) + "}";
                        }
                        return m_engine.string(json + "]");
                    });
    m_engine.define("__paint.channelRange", [this](const std::vector<Value>& arguments) {
        const auto range{channel_range(image(arguments.at(0)))};
        const auto list{[](const std::array<double, 3>& values)
                        { return "[" + json_number(values[0]) + "," + json_number(values[1]) + "," + json_number(values[2]) + "]"; }};
        return m_engine.string("{\"low\":" + list(range.low) + ",\"high\":" + list(range.high) + "}");
    });
    m_engine.define("__paint.medianCut", [this, number](const std::vector<Value>& arguments) {
        return m_engine.string(
            json_of_palette(median_cut(image(arguments.at(0)), static_cast<int>(number(arguments, 1)))));
    });
    define_image("nearest", [this](const std::vector<Value>& arguments) {
        return nearest_indices(image(arguments.at(0)), palette_of_numbers(table_of(arguments.at(1))));
    });
    define_image("ditherFloydSteinberg", [this, number](const std::vector<Value>& arguments) {
        return floyd_steinberg_indices(image(arguments.at(0)), palette_of_numbers(table_of(arguments.at(1))),
                                       number(arguments, 2));
    });
    define_image("ditherBayer", [this, number](const std::vector<Value>& arguments) {
        return bayer_indices(image(arguments.at(0)), palette_of_numbers(table_of(arguments.at(1))),
                             static_cast<int>(number(arguments, 2)), number(arguments, 3));
    });
    define_image("grayscale", [this, number](const std::vector<Value>& arguments) {
        return grayscale(image(arguments.at(0)), number(arguments, 1), number(arguments, 2), number(arguments, 3));
    });
    define_image("ditherLevels", [this, number](const std::vector<Value>& arguments) {
        return dither_levels(image(arguments.at(0)), static_cast<int>(number(arguments, 1)),
                             static_cast<int>(number(arguments, 2)), number(arguments, 3));
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
    if(m_lent != nullptr && static_cast<int>(id.to_number()) == m_lent_id)
    {
        return *m_lent;
    }
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

std::unique_ptr<PluginHost> PluginHost::copy() const
{
    auto made{std::make_unique<PluginHost>()};
    made->set_time_limit(m_time_limit);
    if(m_print)
    {
        made->set_print(m_print);
    }
    for(const auto& [code, name] : m_sources)
    {
        made->load(code, name);
    }
    return made;
}

const Effect& PluginHost::same_effect(const Effect& effect) const
{
    if(effect.index >= m_effects.size() || m_effects[effect.index].name != effect.name)
    {
        throw ScriptError{"the plugins loaded again did not declare " + effect.name + " where they did"};
    }
    return m_effects[effect.index];
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
                        const auto worker{copy()};
                        WorkImage rows{source.width, last - first};
                        std::copy(source.at(0, first), source.at(0, first) + rows.pixels.size(), rows.pixels.begin());
                        const auto made{worker->apply_rows(worker->same_effect(effect), rows, first, json, context)};
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
    // Lent, not copied: every piece makes a new image of what it reads.
    m_lent = &source;
    m_lent_id = m_next_id++;
    struct Return
    {
        PluginHost& host;
        ~Return()
        {
            host.m_lent = nullptr;
            host.m_images.clear();
        }
    } given_back{*this};
    const auto result{m_runner.call({m_engine.number(static_cast<double>(effect.index)), m_engine.number(m_lent_id),
                                     m_engine.string(json), m_engine.string(context_json)})};
    const auto id{static_cast<int>(result.to_number())};
    if(id == m_lent_id)
    {
        // Given back as it came.
        return source;
    }
    const auto found{m_images.find(id)};
    if(found == m_images.end())
    {
        throw ScriptError{effect.name + " gave back no image"};
    }
    auto made{std::move(found->second)};
    if(made.width != source.width || made.height != source.height)
    {
        throw ScriptError{effect.name + " gave back an image of another size"};
    }
    return made;
}

namespace
{

std::string json_of_values(const std::map<std::string, double>& values)
{
    std::string json{"{"};
    for(const auto& [id, value] : values)
    {
        json += (json.size() > 1 ? "," : "") + json_string(id) + ":" + json_number(value);
    }
    return json + "}";
}

} // namespace

Indexed PluginHost::index(const Indexer& indexer, const WorkImage& source, const std::map<std::string, double>& values,
                          const IndexContext& context)
{
    const auto context_json{"{\"count\":" + std::to_string(context.count) + ",\"palette\":" +
                            (context.palette ? json_of_palette(*context.palette) : std::string{"null"}) + "}"};
    m_images.clear();
    m_lent = &source;
    m_lent_id = m_next_id++;
    struct Return
    {
        PluginHost& host;
        ~Return()
        {
            host.m_lent = nullptr;
            host.m_images.clear();
        }
    } given_back{*this};
    const auto result{m_index_runner.call({m_engine.number(static_cast<double>(indexer.index)),
                                           m_engine.number(m_lent_id), m_engine.string(json_of_values(values)),
                                           m_engine.string(context_json)})};
    const auto parsed{parse_json(result.to_string())};
    if(!parsed || parsed->kind != Json::Kind::object)
    {
        throw ScriptError{indexer.name + " gave back no palette and indices"};
    }
    Indexed made;
    const auto& palette{(*parsed)["palette"]};
    for(std::size_t entry{}; entry < palette.items.size(); ++ entry)
    {
        const auto& color{palette.items[entry]};
        if(color.items.size() < 3)
        {
            throw ScriptError{indexer.name + "'s palette has a colour that is not three numbers from 0 to 1"};
        }
        made.palette.push_back({std::clamp(color.items[0].number, 0.0, 1.0),
                                std::clamp(color.items[1].number, 0.0, 1.0),
                                std::clamp(color.items[2].number, 0.0, 1.0), 1.0});
    }
    if(static_cast<int>(made.palette.size()) > context.count)
    {
        throw ScriptError{indexer.name + " made " + std::to_string(made.palette.size()) + " colours, " +
                          std::to_string(context.count) + " being all there is room for"};
    }
    const auto id{static_cast<int>((*parsed)["indices"].number)};
    const auto found{m_images.find(id)};
    if(found == m_images.end())
    {
        throw ScriptError{indexer.name + " gave back no image of indices"};
    }
    made.indices = std::move(found->second);
    if(made.indices.width != source.width || made.indices.height != source.height)
    {
        throw ScriptError{indexer.name + " gave back indices of another size"};
    }
    for(std::size_t at{}; at < made.indices.pixels.size(); at += 4)
    {
        if(made.indices.pixels[at + 3] >= 0.5f &&
           std::lround(made.indices.pixels[at] * 255.0f) >= static_cast<long>(made.palette.size()))
        {
            throw ScriptError{indexer.name + " gave a pixel an index past its palette"};
        }
    }
    return made;
}

WorkImage PluginHost::convert(const Converter& converter, const WorkImage& source,
                              const std::map<std::string, double>& values)
{
    m_images.clear();
    m_lent = &source;
    m_lent_id = m_next_id++;
    struct Return
    {
        PluginHost& host;
        ~Return()
        {
            host.m_lent = nullptr;
            host.m_images.clear();
        }
    } given_back{*this};
    const auto result{m_convert_runner.call({m_engine.number(static_cast<double>(converter.index)),
                                             m_engine.number(m_lent_id), m_engine.string(json_of_values(values))})};
    const auto id{static_cast<int>(result.to_number())};
    if(id == m_lent_id)
    {
        return source;
    }
    const auto found{m_images.find(id)};
    if(found == m_images.end())
    {
        throw ScriptError{converter.name + " gave back no image"};
    }
    auto made{std::move(found->second)};
    if(made.width != source.width || made.height != source.height)
    {
        throw ScriptError{converter.name + " gave back an image of another size"};
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

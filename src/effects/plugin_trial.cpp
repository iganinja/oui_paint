#include "effects/plugin_trial.h"

#include <algorithm>
#include <mutex>

namespace oui_paint
{

std::optional<std::size_t> error_line_in(std::string_view error, std::string_view file_name)
{
    const auto name{std::string{file_name} + ":"};
    for(auto at{error.find(name)}; at != std::string_view::npos; at = error.find(name, at + 1))
    {
        std::size_t line{};
        auto digits{false};
        for(auto index{at + name.size()}; index < error.size() && error[index] >= '0' && error[index] <= '9'; ++ index)
        {
            line = line * 10 + static_cast<std::size_t>(error[index] - '0');
            digits = true;
        }
        if(digits && line > 0)
        {
            return line;
        }
    }
    return std::nullopt;
}

PluginTrial try_plugin(std::string_view source, std::string_view file_name, std::string_view effect_name,
                       const Image& image, const std::map<std::string, double>& values, const EffectContext& context,
                       std::chrono::milliseconds limit)
{
    PluginTrial trial;
    std::mutex log_lock;
    PluginHost host;
    host.set_time_limit(limit);
    host.set_print(
        [&trial, &log_lock](std::string_view text)
        {
            const std::lock_guard lock{log_lock};
            trial.log.append(text).append("\n");
        });
    try
    {
        host.load(source, file_name);
        trial.effects = host.effects();
        if(trial.effects.empty())
        {
            return trial;
        }
        std::size_t chosen{};
        for(std::size_t index{}; index < trial.effects.size(); ++ index)
        {
            if(trial.effects[index].name == effect_name)
            {
                chosen = index;
            }
        }
        trial.tried = chosen;
        const auto& effect{trial.effects[chosen]};
        std::map<std::string, double> used;
        for(const auto& parameter : effect.parameters)
        {
            const auto given{values.find(parameter.id)};
            used[parameter.id] = given != values.end() ? std::clamp(given->second, parameter.minimum, parameter.maximum)
                                                       : parameter.value;
        }
        trial.made = from_work(host.apply(effect, to_work(converted(image, PixelFormat::rgba8)), used, context),
                               PixelFormat::rgba8);
    }
    catch(const std::exception& failure)
    {
        trial.error = failure.what();
        trial.error_line = error_line_in(*trial.error, file_name);
    }
    return trial;
}

std::string_view new_plugin_source()
{
    return R"js(// A plugin: the effects it declares appear in the Effects menu once it is saved in the plugins folder.
// It runs again as you type, on the image beside it.

paint.effect({
    name: "My Effect",
    menu: "Stylize",
    parameters: [
        {id: "radius", name: "Radius", min: 0, max: 50, value: 4},
        {id: "amount", name: "Amount", min: 0, max: 100, value: 60},
    ],
    render(image, values, context) {
        const soft = image.gaussianBlur(values.radius);
        return soft.over(image, "Normal", values.amount / 100);
    },
});
)js";
}

} // namespace oui_paint

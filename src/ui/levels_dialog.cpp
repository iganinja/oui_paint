#include "ui/levels_dialog.h"

#include "ui/levels_bar.h"

#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/window.h>

#include <algorithm>
#include <cmath>
#include <numeric>

using namespace olagarro;

namespace oui_paint
{

double gamma_marker(double input_low, double input_high, double gamma)
{
    return (input_low + (input_high - input_low) * std::pow(0.5, gamma)) / 255.0;
}

double gamma_at(double input_low, double input_high, double place)
{
    const auto span{std::max(1.0, input_high - input_low)};
    const auto along{std::clamp((place * 255.0 - input_low) / span, 0.01, 0.99)};
    return std::clamp(std::log(along) / std::log(0.5), 0.1, 10.0);
}

std::pair<int, int> auto_levels(const std::array<double, 256>& histogram)
{
    const auto total{std::accumulate(histogram.begin(), histogram.end(), 0.0)};
    if(total <= 0.0)
    {
        return {0, 255};
    }
    const auto clipped{total * 0.005};
    auto low{0};
    for(auto seen{0.0}; low < 255 && seen + histogram[static_cast<std::size_t>(low)] <= clipped; ++ low)
    {
        seen += histogram[static_cast<std::size_t>(low)];
    }
    auto high{255};
    for(auto seen{0.0}; high > low + 1 && seen + histogram[static_cast<std::size_t>(high)] <= clipped; -- high)
    {
        seen += histogram[static_cast<std::size_t>(high)];
    }
    return {low, std::max(high, low + 1)};
}

bool ask_levels(oui::Application& application, std::map<std::string, double>& values,
                const std::function<void(const std::map<std::string, double>&)>& changed, const Histogram& histogram)
{
    struct Channel
    {
        const char* name;
        const char* label;
        const std::array<double, 256>* histogram;
    };
    const std::array<Channel, 3> channels{{{"r", "&Red", &histogram.red},
                                           {"g", "G&reen", &histogram.green},
                                           {"b", "Bl&ue", &histogram.blue}}};
    const std::array<const char*, 5> ids{"inputLow", "inputHigh", "gamma", "outputLow", "outputHigh"};

    auto window{oui::window("Levels")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);

    auto& heading{oui::row(content)};
    heading.set_gap(oui::Spacing::tight);
    oui::label("Input", heading);
    std::array<oui::CheckButton*, 3> ticks{};
    for(std::size_t index{}; index < channels.size(); ++ index)
    {
        ticks[index] = &oui::check_button(channels[index].label, heading);
        ticks[index]->set_checked(true);
    }
    auto& input{content.add(std::make_unique<LevelsBar>(6))};
    oui::label("Output", content);
    auto& output{content.add(std::make_unique<LevelsBar>(2))};

    auto& form{oui::grid(content)};
    form.set_columns({oui::Length::content(), oui::Length::content(), oui::Length::content(), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);
    struct Field
    {
        const char* id;
        const char* label;
        double minimum;
        double maximum;
        int decimals;
    };
    const std::array<Field, 5> described{{{"inputLow", "Input &black:", 0, 255, 0},
                                          {"inputHigh", "Input &white:", 0, 255, 0},
                                          {"gamma", "&Gamma:", 0.1, 10, 2},
                                          {"outputLow", "Output b&lack:", 0, 255, 0},
                                          {"outputHigh", "Output w&hite:", 0, 255, 0}}};
    std::map<std::string, oui::NumberField*> fields;
    for(const auto& field : described)
    {
        auto& label{oui::label(field.label, form)};
        auto& number{oui::number_field(form)};
        number.set_range(field.minimum, field.maximum);
        number.set_step(field.decimals == 0 ? 1.0 : 0.0);
        number.set_decimals(field.decimals);
        label.set_buddy(number.text_field());
        fields[field.id] = &number;
    }

    // The colours ticked, those the dialog edits together; the first is the one it shows.
    const auto ticked{[&]
                      {
                          std::vector<std::size_t> found;
                          for(std::size_t index{}; index < channels.size(); ++ index)
                          {
                              if(ticks[index]->is_checked())
                              {
                                  found.push_back(index);
                              }
                          }
                          return found;
                      }};
    // A value of a colour: its own, or the one all the colours share until they have their own.
    const auto value_of{[&values](const std::string& id, const char* channel)
                        {
                            const auto own{values.find(id + "_" + channel)};
                            return own != values.end() ? own->second : values[id];
                        }};
    const auto shown_channel{[&]() -> const char*
                             {
                                 const auto found{ticked()};
                                 return found.empty() ? "r" : channels[found.front()].name;
                             }};
    const auto place_markers{[&]
                             {
                                 const auto* const channel{shown_channel()};
                                 const auto low{value_of("inputLow", channel)};
                                 const auto high{value_of("inputHigh", channel)};
                                 input.set_markers({{low / 255.0, 0x000000},
                                                    {gamma_marker(low, high, value_of("gamma", channel)), 0x808080},
                                                    {high / 255.0, 0xFFFFFF}});
                                 output.set_markers({{value_of("outputLow", channel) / 255.0, 0x000000},
                                                     {value_of("outputHigh", channel) / 255.0, 0xFFFFFF}});
                             }};
    // The fields and the histogram as the colours ticked are: the histogram of luminosity when all are.
    auto refreshing{false};
    const auto show{[&]
                    {
                        const auto found{ticked()};
                        input.set_histogram(found.size() == channels.size() || found.empty()
                                                ? histogram.luminosity
                                                : *channels[found.front()].histogram);
                        refreshing = true;
                        for(const auto* const id : ids)
                        {
                            fields[id]->set_value(value_of(id, shown_channel()));
                        }
                        refreshing = false;
                        place_markers();
                    }};
    for(const auto& [id, field] : fields)
    {
        static_cast<void>(field->on_changed(
            [&, id{id}](double value)
            {
                if(refreshing)
                {
                    return;
                }
                for(const auto index : ticked())
                {
                    values[id + "_" + channels[index].name] = value;
                }
                place_markers();
                changed(values);
            }));
    }
    for(auto* const tick : ticks)
    {
        static_cast<void>(tick->on_toggled([&](bool) { show(); }));
    }
    // Black stays below white by a level; moving either keeps the grey marker's gamma.
    input.on_moved(
        [&](std::size_t marker, double place)
        {
            const auto* const channel{shown_channel()};
            const auto level{std::round(place * 255.0)};
            if(marker == 0)
            {
                fields["inputLow"]->set_value(std::min(level, value_of("inputHigh", channel) - 1.0));
            }
            else if(marker == 2)
            {
                fields["inputHigh"]->set_value(std::max(level, value_of("inputLow", channel) + 1.0));
            }
            else
            {
                fields["gamma"]->set_value(
                    gamma_at(value_of("inputLow", channel), value_of("inputHigh", channel), place));
            }
        });
    output.on_moved([&](std::size_t marker, double place)
                    { fields[marker == 0 ? "outputLow" : "outputHigh"]->set_value(std::round(place * 255.0)); });

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("&Auto", buttons,
                [&]
                {
                    // Each colour ticked stretched by its own histogram.
                    for(const auto index : ticked())
                    {
                        const auto [low, high]{auto_levels(*channels[index].histogram)};
                        const std::string name{channels[index].name};
                        values["inputLow_" + name] = low;
                        values["inputHigh_" + name] = high;
                        values["gamma_" + name] = 1.0;
                    }
                    show();
                    changed(values);
                });
    oui::button("Re&set", buttons,
                [&]
                {
                    for(const auto& channel : channels)
                    {
                        for(const auto* const id : ids)
                        {
                            values.erase(std::string{id} + "_" + channel.name);
                        }
                    }
                    values["inputLow"] = 0;
                    values["inputHigh"] = 255;
                    values["gamma"] = 1.0;
                    values["outputLow"] = 0;
                    values["outputHigh"] = 255;
                    show();
                    changed(values);
                });
    oui::button("OK", buttons,
                [&]
                {
                    for(auto& [id, field] : fields)
                    {
                        field->commit();
                    }
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    show();
    static_cast<void>(application.show_modal_and_keep(window));
    return accepted;
}

} // namespace oui_paint

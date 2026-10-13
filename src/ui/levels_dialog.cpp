#include "ui/levels_dialog.h"

#include "ui/levels_bar.h"

#include <oui/button.h>
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
    auto window{oui::window("Levels")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);

    oui::label("Input", content);
    auto& input{content.add(std::make_unique<LevelsBar>(6))};
    input.set_histogram(histogram.luminosity);
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
        number.set_value(values[field.id]);
        label.set_buddy(number.text_field());
        fields[field.id] = &number;
    }

    // The markers where the values put them.
    const auto place_markers{[&]
                             {
                                 const auto low{values["inputLow"]};
                                 const auto high{values["inputHigh"]};
                                 input.set_markers({{low / 255.0, 0x000000},
                                                    {gamma_marker(low, high, values["gamma"]), 0x808080},
                                                    {high / 255.0, 0xFFFFFF}});
                                 output.set_markers(
                                     {{values["outputLow"] / 255.0, 0x000000}, {values["outputHigh"] / 255.0, 0xFFFFFF}});
                             }};
    for(const auto& [id, field] : fields)
    {
        static_cast<void>(field->on_changed(
            [&values, &changed, &place_markers, id{id}](double value)
            {
                values[id] = value;
                place_markers();
                changed(values);
            }));
    }
    // Black stays below white by a level; moving either keeps the grey marker's gamma.
    input.on_moved(
        [&](std::size_t marker, double place)
        {
            const auto level{std::round(place * 255.0)};
            if(marker == 0)
            {
                fields["inputLow"]->set_value(std::min(level, values["inputHigh"] - 1.0));
            }
            else if(marker == 2)
            {
                fields["inputHigh"]->set_value(std::max(level, values["inputLow"] + 1.0));
            }
            else
            {
                fields["gamma"]->set_value(gamma_at(values["inputLow"], values["inputHigh"], place));
            }
        });
    output.on_moved(
        [&](std::size_t marker, double place)
        {
            fields[marker == 0 ? "outputLow" : "outputHigh"]->set_value(std::round(place * 255.0));
        });

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("&Auto", buttons,
                [&]
                {
                    const auto [low, high]{auto_levels(histogram.luminosity)};
                    fields["inputLow"]->set_value(low);
                    fields["inputHigh"]->set_value(high);
                    fields["gamma"]->set_value(1.0);
                });
    oui::button("Re&set", buttons,
                [&]
                {
                    fields["inputLow"]->set_value(0);
                    fields["inputHigh"]->set_value(255);
                    fields["gamma"]->set_value(1.0);
                    fields["outputLow"]->set_value(0);
                    fields["outputHigh"]->set_value(255);
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
                });
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    place_markers();
    static_cast<void>(application.show_modal_and_keep(window));
    return accepted;
}

} // namespace oui_paint

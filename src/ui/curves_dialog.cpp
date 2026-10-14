#include "ui/curves_dialog.h"

#include "ui/curve_editor.h"

#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/window.h>

#include <array>
#include <vector>

using namespace olagarro;

namespace oui_paint
{

bool ask_curves(oui::Application& application, std::map<std::string, double>& values,
                const std::function<void(const std::map<std::string, double>&)>& changed, const Histogram& histogram)
{
    struct Channel
    {
        const char* name;
        const char* label;
        std::uint32_t color;
        const std::array<double, 256>* histogram;
    };
    const std::array<Channel, 3> channels{{{"r", "&Red", 0xE04848, &histogram.red},
                                           {"g", "&Green", 0x48C048, &histogram.green},
                                           {"b", "&Blue", 0x5878F0, &histogram.blue}}};

    auto window{oui::window("Curves")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& top{oui::row(content)};
    top.set_gap(oui::Spacing::tight);
    auto& map_label{oui::label("&Transfer map:", top)};
    auto& map{top.add(std::make_unique<oui::ComboBox>(std::vector<std::string>{"Luminosity", "RGB"},
                                                      values["mode"] == 1.0 ? 1u : 0u))};
    map_label.set_buddy(map);
    std::array<oui::CheckButton*, 3> ticks{};
    for(std::size_t index{}; index < channels.size(); ++ index)
    {
        ticks[index] = &oui::check_button(channels[index].label, top);
        ticks[index]->set_checked(true);
    }
    auto& editor{content.add(std::make_unique<CurveEditor>())};
    oui::label("Click to add a point and drag to move it;", content);
    oui::label("a right click, or dragging it far off, takes it away.", content);

    // The curves edited: luminosity's, or those of the colours ticked.
    const auto edited{[&]
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
    // The graph as the map and the ticks say: the first curve edited on it, the colours not edited faint behind.
    const auto show{[&]
                    {
                        const auto rgb{map.selected() == 1};
                        for(auto* const tick : ticks)
                        {
                            tick->set_visible(rgb);
                        }
                        if(!rgb)
                        {
                            editor.set_points(curve_points(values, "l"));
                            editor.set_color(0xE8E8E0);
                            editor.set_faint({});
                            editor.set_histogram(histogram.luminosity);
                            return;
                        }
                        const auto ticked{edited()};
                        std::vector<std::pair<std::vector<CurvePoint>, std::uint32_t>> faint;
                        for(std::size_t index{}; index < channels.size(); ++ index)
                        {
                            if(ticked.empty() || index != ticked.front())
                            {
                                faint.emplace_back(curve_points(values, channels[index].name), channels[index].color);
                            }
                        }
                        editor.set_faint(std::move(faint));
                        if(!ticked.empty())
                        {
                            const auto& first{channels[ticked.front()]};
                            editor.set_points(curve_points(values, first.name));
                            editor.set_color(ticked.size() == 1 ? first.color : 0xE8E8E0);
                            editor.set_histogram(*first.histogram);
                        }
                    }};
    editor.on_changed(
        [&]
        {
            if(map.selected() == 0)
            {
                put_curve(values, "l", editor.points());
            }
            else
            {
                for(const auto index : edited())
                {
                    put_curve(values, channels[index].name, editor.points());
                }
            }
            changed(values);
        });
    static_cast<void>(map.on_selected(
        [&](std::size_t index)
        {
            values["mode"] = static_cast<double>(index);
            show();
            changed(values);
        }));
    for(auto* const tick : ticks)
    {
        static_cast<void>(tick->on_toggled([&](bool) { show(); }));
    }

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("Re&set", buttons,
                [&]
                {
                    for(const auto* const name : {"l", "r", "g", "b"})
                    {
                        put_curve(values, name, {{0.0, 0.0}, {1.0, 1.0}});
                    }
                    show();
                    changed(values);
                });
    oui::button("OK", buttons,
                [&]
                {
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    show();
    static_cast<void>(application.show_modal_and_keep(window));
    return accepted;
}

} // namespace oui_paint

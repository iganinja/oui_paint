#include "ui/mode_dialogs.h"

#include "ui/effect_controls.h"

#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/window.h>

#include <algorithm>
#include <memory>

using namespace olagarro;

namespace oui_paint
{

void MethodChoice::offer(std::vector<std::string> new_names, std::vector<std::vector<EffectParameter>> new_parameters)
{
    const auto was{chosen < names.size() ? names[chosen] : std::string{}};
    names = std::move(new_names);
    parameters = std::move(new_parameters);
    const auto found{std::find(names.begin(), names.end(), was)};
    chosen = found != names.end() ? static_cast<std::size_t>(found - names.begin()) : 0;
}

std::map<std::string, double> MethodChoice::chosen_values()
{
    if(chosen >= names.size())
    {
        return {};
    }
    auto& values{this->values[names[chosen]]};
    for(const auto& parameter : parameters[chosen])
    {
        values.try_emplace(parameter.id, parameter.value);
    }
    return values;
}

namespace
{

// The image follows the dialog once a frame, with the choice as it is by then, and what went wrong shows under it.
class Follower
{
  public:
    Follower(oui::Application& application, std::function<std::string()> changed, oui::Label& problem)
        : m_application{application}, m_changed{std::move(changed)}, m_problem{problem}
    {
    }
    Follower(const Follower&) = delete;
    Follower& operator=(const Follower&) = delete;
    ~Follower() { m_state->closed = true; }

    void follow()
    {
        if(m_state->pending)
        {
            return;
        }
        m_state->pending = true;
        m_application.post(
            [state{m_state}, this]
            {
                if(state->closed)
                {
                    return;
                }
                state->pending = false;
                // Its first line: a plugin's error goes on with where in the code it was thrown.
                const auto problem{m_changed()};
                m_problem.set_text(problem.substr(0, problem.find('\n')));
            });
    }

  private:
    struct State
    {
        bool pending{false};
        bool closed{false};
    };
    oui::Application& m_application;
    std::function<std::string()> m_changed;
    oui::Label& m_problem;
    std::shared_ptr<State> m_state{std::make_shared<State>()};
};

// A grid of three columns, a label, a control and what goes after, as the effects' dialogs have.
oui::Grid& form_in(oui::Container& parent)
{
    auto& form{oui::grid(parent)};
    form.set_columns({oui::Length::content(), oui::Length::cells(24), oui::Length::content()});
    form.set_gap(oui::Spacing::tight);
    return form;
}

// Rows of `form`, the last it has: a list of the methods, and under it the chosen one's parameters, made again when
// another is chosen.
class MethodControls
{
  public:
    MethodControls(oui::Grid& form, MethodChoice& choice, std::function<void()> changed)
        : m_choice{choice}, m_changed{std::move(changed)}, m_form{&form}, m_first{form.child_count() + 3}
    {
        auto& label{oui::label("&Method:", *m_form)};
        auto& methods{m_form->add(oui::combo_box(choice.names, choice.chosen))};
        label.set_buddy(methods);
        oui::label("", *m_form);
        static_cast<void>(methods.on_selected(
            [this](std::size_t index)
            {
                m_choice.chosen = index;
                rebuild();
                m_changed();
            }));
        rebuild();
    }

    void commit()
    {
        for(auto* const field : m_fields)
        {
            field->commit();
        }
    }

  private:
    void rebuild()
    {
        while(m_form->child_count() > m_first)
        {
            static_cast<void>(m_form->remove_at(m_first));
        }
        m_fields.clear();
        if(m_choice.chosen >= m_choice.names.size())
        {
            return;
        }
        static_cast<void>(m_choice.chosen_values());
        Effect shown;
        shown.name = m_choice.names[m_choice.chosen];
        shown.parameters = m_choice.parameters[m_choice.chosen];
        m_fields = add_effect_controls(*m_form, shown, m_choice.values[shown.name], m_changed);
    }

    MethodChoice& m_choice;
    std::function<void()> m_changed;
    oui::Grid* m_form{};
    // Where the parameters' rows begin.
    std::size_t m_first{0};
    std::vector<oui::NumberField*> m_fields;
};

oui::Label& problem_label(oui::Container& parent)
{
    auto& problem{oui::label("", parent)};
    problem.set_color(&oui::Theme::error_foreground);
    return problem;
}

// OK and Cancel, OK committing what was being typed first; `first`, when given, puts buttons before them.
bool add_buttons_and_show(oui::Application& application, std::unique_ptr<oui::Window> window, oui::Container& content,
                          const std::function<void()>& commit,
                          const std::function<void(oui::Container&)>& first = {})
{
    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    if(first)
    {
        first(buttons);
    }
    auto& window_reference{*window};
    auto accepted{false};
    oui::button("OK", buttons,
                [&]
                {
                    commit();
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button("Cancel", buttons, [&] { window_reference.close(oui::DialogResult::cancelled); });
    static_cast<void>(application.show_modal_and_keep(window));
    return accepted;
}

} // namespace

bool ask_indexing(oui::Application& application, IndexingChoice& choice,
                  const std::function<std::optional<std::pair<std::string, std::vector<Color>>>()>& load,
                  const std::function<std::string()>& changed)
{
    auto window{oui::window("Convert to Indexed")};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);

    auto& form{form_in(content)};
    const auto palette_names{[&choice]
                             {
                                 std::vector<std::string> names{"Make one"};
                                 if(choice.loaded)
                                 {
                                     names.push_back(choice.loaded->first);
                                 }
                                 return names;
                             }};
    auto& palette_label{oui::label("&Palette:", form)};
    auto& palettes{form.add(oui::combo_box(palette_names(), choice.into_loaded && choice.loaded ? 1 : 0))};
    palette_label.set_buddy(palettes);
    oui::label("", form);
    auto& count_label{oui::label("&Colors:", form)};
    auto& count{oui::number_field(form)};
    count.set_range(2, 256);
    count.set_step(1);
    count.set_decimals(0);
    count.set_value(choice.count);
    count_label.set_buddy(count.text_field());
    oui::label("", form);

    auto& problem{problem_label(content)};
    Follower follower{application, changed, problem};
    const auto follow{[&follower] { follower.follow(); }};
    MethodControls method{form, choice.method, follow};
    // The count is for a palette made: one loaded has as many as it has.
    const auto show_palette{[&]
                            {
                                choice.into_loaded = choice.loaded && palettes.selected() == 1;
                                count.set_enabled(!choice.into_loaded);
                            }};
    show_palette();
    static_cast<void>(palettes.on_selected(
        [&](std::size_t)
        {
            show_palette();
            follow();
        }));
    const auto load_palette{[&]
        {
            if(auto loaded{load()})
            {
                choice.loaded = std::move(loaded);
                palettes.set_options(palette_names());
                palettes.set_selected(1);
                show_palette();
                follow();
            }
        }};
    static_cast<void>(count.on_changed(
        [&](double value)
        {
            choice.count = static_cast<int>(value);
            follow();
        }));
    follow();
    return add_buttons_and_show(application, std::move(window), content,
                                [&]
                                {
                                    count.commit();
                                    method.commit();
                                },
                                [&](oui::Container& buttons) { oui::button("&Load Palette...", buttons, load_palette); });
}

bool ask_conversion(oui::Application& application, const std::string& title,
                    const std::vector<std::pair<std::string, MethodChoice*>>& steps,
                    const std::function<std::string()>& changed)
{
    auto window{oui::window(title)};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& problem{problem_label(content)};
    Follower follower{application, changed, problem};
    std::vector<std::unique_ptr<MethodControls>> methods;
    for(const auto& [heading, choice] : steps)
    {
        if(steps.size() > 1)
        {
            oui::label(heading, content).set_bold(true);
        }
        methods.push_back(
            std::make_unique<MethodControls>(form_in(content), *choice, [&follower] { follower.follow(); }));
    }
    content.add(content.remove_at(0));
    follower.follow();
    return add_buttons_and_show(application, std::move(window), content,
                                [&]
                                {
                                    for(const auto& method : methods)
                                    {
                                        method->commit();
                                    }
                                });
}

} // namespace oui_paint

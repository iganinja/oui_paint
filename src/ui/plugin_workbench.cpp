#include "ui/plugin_workbench.h"

#include "app/plugin_api.h"
#include "effects/test_card.h"
#include "ui/canvas_view.h"
#include "ui/effect_controls.h"

#include <oui/translation.h>
#include <oui/file_system.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/script/rule_highlighter.h>
#include <oui/split_view.h>

#include <algorithm>

using namespace olagarro;
using namespace std::chrono_literals;

namespace oui_paint
{

namespace
{

// How long a run may take before it counts as stuck, as a loop that never ends.
constexpr auto trial_limit{3s};
// How long after the last change it runs again.
constexpr auto settle_time{300ms};

// A single widget filling it, which can be swapped for another.
class Holder final : public oui::Container
{
  public:
    oui::Size measure(const oui::Constraints& constraints) const override
    {
        return {constraints.max_width, constraints.max_height};
    }
    void arrange(const oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        for(const auto& child : children())
        {
            child->arrange(bounds);
        }
    }
};

[[nodiscard]] bool same_parameters(const std::vector<EffectParameter>& a, const std::vector<EffectParameter>& b)
{
    return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                      [](const EffectParameter& left, const EffectParameter& right)
                      {
                          return left.id == right.id && left.name == right.name && left.minimum == right.minimum &&
                                 left.maximum == right.maximum && left.step == right.step &&
                                 left.options == right.options && left.value == right.value;
                      });
}

} // namespace

std::shared_ptr<const oui::SyntaxHighlighter> javascript_highlighter()
{
    using oui::SyntaxKind;
    using oui::script::HighlightRule;
    using oui::script::HighlightState;
    static const auto highlighter{std::make_shared<const oui::script::RuleHighlighter>(std::vector<HighlightState>{
        {"start",
         SyntaxKind::plain,
         {
             {R"(//.*)", "", SyntaxKind::comment, "", false},
             {R"(/\*)", "", std::nullopt, "block_comment", false},
             {R"(")", "", std::nullopt, "double_string", false},
             {R"(')", "", std::nullopt, "single_string", false},
             {R"(`)", "", std::nullopt, "template", false},
             {R"(\b(?:0[xX][\da-fA-F_]+|\d[\d_]*(?:\.[\d_]*)?(?:[eE][+-]?\d+)?)\b)", "", SyntaxKind::number, "", false},
             {R"(\b(?:async|await|break|case|catch|class|const|continue|default|delete|do|else|export|extends|)"
              R"(finally|for|from|function|if|import|in|instanceof|let|new|of|return|static|super|switch|throw|)"
              R"(try|typeof|var|void|while|yield)\b)",
              "", SyntaxKind::keyword, "", false},
             {R"(\b(?:true|false|null|undefined|NaN|Infinity|this)\b)", "", SyntaxKind::constant, "", false},
             {R"([A-Za-z_$][\w$]*(?=\s*\())", "", SyntaxKind::function, "", false},
             {R"(\})", "", std::nullopt, "", true},
             {R"(\{)", "", std::nullopt, "start", false},
         }},
        {"block_comment", SyntaxKind::comment, {{R"(\*/)", "", std::nullopt, "", true}}},
        {"double_string",
         SyntaxKind::string,
         {{R"(\\.)", "", std::nullopt, "", false},
          {R"(")", "", std::nullopt, "", true},
          {R"($)", "", std::nullopt, "", true}}},
        {"single_string",
         SyntaxKind::string,
         {{R"(\\.)", "", std::nullopt, "", false},
          {R"(')", "", std::nullopt, "", true},
          {R"($)", "", std::nullopt, "", true}}},
        {"template",
         SyntaxKind::string,
         {{R"(\\.)", "", std::nullopt, "", false},
          {R"(`)", "", std::nullopt, "", true},
          {R"(\$\{)", "", SyntaxKind::operator_, "start", false}}},
    })};
    return highlighter;
}

PluginWorkbench::PluginWorkbench(oui::Application& application, std::string source,
                                 std::optional<std::filesystem::path> file)
    : m_application{application}, m_file{std::move(file)}, m_saved_source{m_file ? source : std::string{}},
      m_test_card{test_card()}
{
    auto& split{add(oui::split_view())};
    split.set_ratio(0.45);
    m_code = &split.add(std::make_unique<oui::TextEdit>());
    m_code->set_syntax_highlighter(javascript_highlighter());
    m_code->set_line_numbers_visible(true);
    m_code->set_auto_indent(true);
    m_code->set_use_spaces(true);
    // A paint program's tools are single keys: what is typed here is the code's.
    m_code->set_claims_typing(true);
    m_code->set_text(source);
    static_cast<void>(m_code->on_changed(
        [this]
        {
            if(modified() != m_was_modified)
            {
                m_was_modified = modified();
                if(m_title_changed)
                {
                    m_title_changed();
                }
            }
            schedule();
        }));

    auto& right{split.add(oui::grid())};
    right.set_columns({oui::Length::fill()});
    right.set_rows({oui::Length::content(), oui::Length::fill(), oui::Length::content(), oui::Length::cells(6),
                    oui::Length::content()});
    right.set_gap(oui::Spacing::tight);

    // The lists take what the labels and the button leave.
    auto& choices{oui::grid(right)};
    choices.set_columns({oui::Length::content(), oui::Length::fill(), oui::Length::content()});
    choices.set_gap(oui::Spacing::tight);
    auto& effect_label{oui::label(oui::translate("Dialogs", "Effec&t:"), choices)};
    m_effects = &choices.add(std::make_unique<oui::ComboBox>(std::vector<std::string>{"(none)"}));
    effect_label.set_buddy(*m_effects);
    oui::label("", choices);
    static_cast<void>(m_effects->on_selected(
        [this](std::size_t index)
        {
            if(m_trial && index < m_trial->effects.size())
            {
                m_effect_name = m_trial->effects[index].name;
                run_now();
            }
        }));
    auto& image_label{oui::label(oui::translate("Dialogs", "Tr&y on:"), choices)};
    m_images = &choices.add(std::make_unique<oui::ComboBox>(std::vector<std::string>{oui::translate("Dialogs", "Test Card")}));
    image_label.set_buddy(*m_images);
    static_cast<void>(m_images->on_selected(
        [this](std::size_t index)
        {
            m_chosen_name = index == 0 ? std::string{}
                            : index <= m_open_names.size() ? m_open_names[index - 1]
                                                           : std::string{"\x01loaded"};
            run_now();
        }));
    oui::button(oui::translate("Dialogs", "Loa&d Image..."), choices,
                [this]
                {
                    if(m_load_image)
                    {
                        m_load_image();
                    }
                });

    m_preview_holder = &right.add(std::make_unique<Holder>());
    m_controls = &oui::grid(right);
    m_controls->set_columns({oui::Length::content(), oui::Length::fill(), oui::Length::content()});
    m_controls->set_gap(oui::Spacing::tight);
    m_log = &right.add(std::make_unique<oui::TextEdit>());
    m_log->set_read_only(true);
    m_error = &oui::button("", right,
                           [this]
                           {
                               if(m_error_line)
                               {
                                   static_cast<void>(m_code->go_to_line(*m_error_line));
                                   m_application.focus(*m_code);
                               }
                           });
    m_error->set_visible(false);
    // What the code can use, as paint.d.ts declares it, while one writes in it.
    m_suggester = std::make_unique<oui::Suggester>(
        m_application,
        [this]() -> std::optional<oui::Suggester::Context>
        {
            if(m_application.focused() != m_code)
            {
                return std::nullopt;
            }
            return oui::Suggester::Context{m_code, &javascript_words(), m_api != nullptr ? m_api.get() : &plugin_api(),
                                           {m_code->text()}, false};
        });
    show_image(m_test_card);
    run_now();
}

PluginWorkbench::~PluginWorkbench()
{
    m_alive.reset();
    if(m_worker.joinable())
    {
        m_worker.join();
    }
}

void PluginWorkbench::saved(std::filesystem::path file)
{
    m_file = std::move(file);
    m_saved_source = std::string{m_code->text()};
    m_was_modified = false;
    if(m_title_changed)
    {
        m_title_changed();
    }
}

bool PluginWorkbench::modified() const
{
    return !m_file || m_code->text() != m_saved_source;
}

std::string PluginWorkbench::title() const
{
    return (m_file ? oui::utf8_of(m_file->filename()) : std::string{oui::translate("Dialogs", "New Plugin")}) + (modified() ? " *" : "");
}

void PluginWorkbench::set_open_images(std::vector<std::string> names,
                                      std::function<std::optional<Image>(std::size_t)> image)
{
    const auto changed{names != m_open_names};
    m_open_names = std::move(names);
    m_open_image = std::move(image);
    if(changed)
    {
        refresh_image_choices();
    }
}

void PluginWorkbench::set_loaded_image(std::string name, Image image)
{
    m_loaded = std::make_pair(std::move(name), std::move(image));
    m_chosen_name = "\x01loaded";
    refresh_image_choices();
    run_now();
}

void PluginWorkbench::refresh_image_choices()
{
    std::vector<std::string> options{oui::translate("Dialogs", "Test Card")};
    std::size_t selected{};
    for(std::size_t index{}; index < m_open_names.size(); ++ index)
    {
        options.push_back(m_open_names[index]);
        if(m_open_names[index] == m_chosen_name)
        {
            selected = options.size() - 1;
        }
    }
    if(m_loaded)
    {
        options.push_back(m_loaded->first);
        if(m_chosen_name == "\x01loaded")
        {
            selected = options.size() - 1;
        }
    }
    if(options != m_images->options())
    {
        m_images->set_options(std::move(options));
    }
    m_images->set_selected(selected);
}

Image PluginWorkbench::chosen_image() const
{
    if(m_chosen_name == "\x01loaded" && m_loaded)
    {
        return m_loaded->second;
    }
    for(std::size_t index{}; index < m_open_names.size(); ++ index)
    {
        if(m_open_names[index] == m_chosen_name && m_open_image)
        {
            if(auto image{m_open_image(index)})
            {
                return std::move(*image);
            }
        }
    }
    return m_test_card;
}

void PluginWorkbench::after_event(const oui::Event& event)
{
    std::optional<char32_t> typed;
    if(const auto* const text{std::get_if<oui::TextEvent>(&event)}; text != nullptr && !text->control && !text->alt)
    {
        typed = text->codepoint;
    }
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)};
       key != nullptr && key->key == oui::Key::space && !key->control && !key->alt)
    {
        typed = U' ';
    }
    // Once the event is handled, as what it typed is then in the code.
    m_application.post(
        [this, alive{std::weak_ptr<int>{m_alive}}, typed]
        {
            if(!alive.expired())
            {
                m_suggester->after_event(typed);
            }
        });
}

void PluginWorkbench::suggest()
{
    m_suggester->suggest();
}

void PluginWorkbench::schedule()
{
    m_pending = m_application.start_timer(settle_time,
                                          [this]
                                          {
                                              m_pending.stop();
                                              start_run();
                                          });
}

void PluginWorkbench::run_now()
{
    m_pending.stop();
    start_run();
}

void PluginWorkbench::start_run()
{
    if(m_running)
    {
        m_again = true;
        return;
    }
    if(m_worker.joinable())
    {
        m_worker.join();
    }
    m_running = true;
    m_tried_on = chosen_image();
    auto source{std::string{m_code->text()}};
    auto file_name{m_file ? oui::utf8_of(m_file->filename()) : std::string{"new_plugin.js"}};
    auto values{m_values[m_effect_name]};
    m_worker = std::thread(
        [this, alive{std::weak_ptr<int>{m_alive}}, source{std::move(source)}, file_name{std::move(file_name)},
         effect{m_effect_name}, values{std::move(values)}, image{m_tried_on}]
        {
            auto trial{try_plugin(source, file_name, effect, image, values, {}, trial_limit)};
            m_application.post(
                [this, alive, trial{std::move(trial)}]() mutable
                {
                    if(alive.expired())
                    {
                        return;
                    }
                    m_running = false;
                    show(std::move(trial));
                    if(m_again)
                    {
                        m_again = false;
                        start_run();
                    }
                });
        });
}

void PluginWorkbench::show(PluginTrial trial)
{
    std::vector<std::string> names;
    for(const auto& effect : trial.effects)
    {
        names.push_back(effect.name);
    }
    if(names.empty())
    {
        names.emplace_back("(none)");
    }
    // A failure before any effect is declared, as a half-typed line, leaves the list as it was; and the list is
    // only set when it changed, which would close it while open.
    if(!(trial.error && trial.effects.empty()) && names != m_effects->options())
    {
        m_effects->set_options(names);
    }
    if(trial.tried)
    {
        const auto& effect{trial.effects[*trial.tried]};
        m_effect_name = effect.name;
        m_effects->set_selected(*trial.tried);
        if(!same_parameters(effect.parameters, m_shown_parameters) || m_api == nullptr)
        {
            rebuild_controls(effect);
            m_api = std::make_unique<oui::ScriptApi>(plugin_api_for(effect));
        }
    }
    else if(!trial.error)
    {
        rebuild_controls(Effect{});
    }
    m_log->set_text(trial.log);
    m_error_line = trial.error_line;
    if(trial.error)
    {
        // Its first line says it; the rest is where it was called from.
        const auto first{trial.error->substr(0, trial.error->find('\n'))};
        m_error->set_text(trial.error_line ? oui::format_text(oui::translate("Dialogs", "Line {0}: {1}"), {std::to_string(*trial.error_line), first}) : first);
        m_error->set_visible(true);
        // What it made last stays, so a half-typed line does not blank the image.
    }
    else
    {
        m_error->set_visible(false);
        show_image(trial.made ? *trial.made : m_tried_on);
    }
    m_trial = std::move(trial);
}

void PluginWorkbench::show_image(const Image& image)
{
    if(m_preview != nullptr && m_preview->document().width() == image.width &&
       m_preview->document().height() == image.height)
    {
        auto& layer{m_preview->document().layers().front()};
        layer.write(converted(image, m_preview->document().format()), 0, 0);
        std::vector<TilePoint> all;
        for(int y{}; y * tile_size < image.height; ++ y)
        {
            for(int x{}; x * tile_size < image.width; ++ x)
            {
                all.push_back({x, y});
            }
        }
        m_preview->refresh(all);
        return;
    }
    while(m_preview_holder->child_count() > 0)
    {
        static_cast<void>(m_preview_holder->remove_at(0));
    }
    auto document{std::make_unique<OpenDocument>(Document::from_image(converted(image, PixelFormat::rgba8)))};
    m_preview_view = &m_preview_holder->add(std::make_unique<CanvasView>(*document));
    m_preview = std::move(document);
}

void PluginWorkbench::rebuild_controls(const Effect& effect)
{
    while(m_controls->child_count() > 0)
    {
        static_cast<void>(m_controls->remove_at(0));
    }
    m_shown_parameters = effect.parameters;
    auto& values{m_values[effect.name]};
    static_cast<void>(add_effect_controls(*m_controls, effect, values, [this] { schedule(); }));
}

oui::Size PluginWorkbench::measure(const oui::Constraints& constraints) const
{
    return {constraints.max_width, constraints.max_height};
}

void PluginWorkbench::arrange(const oui::Rect& bounds)
{
    Widget::arrange(bounds);
    for(const auto& child : children())
    {
        child->arrange(bounds);
    }
}

} // namespace oui_paint

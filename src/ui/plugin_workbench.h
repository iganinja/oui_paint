#pragma once

#include "app/open_document.h"
#include "effects/plugin_trial.h"

#include <oui/application.h>
#include <oui/button.h>
#include <oui/combo_box.h>
#include <oui/container.h>
#include <oui/grid.h>
#include <oui/suggester.h>
#include <oui/text_edit.h>

#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace oui_paint
{

class CanvasView;

// A plugin being written, in a tab: its code on the left and, on the right, what its effect makes of an image, run
// again a moment after each change to the code, to its parameters or to the image. Under the image, the effect's
// parameters, what the plugin printed, and why it failed, which a click takes to its line. The image is the test card,
// an open image, or one loaded from a file.
class PluginWorkbench final : public olagarro::oui::Container
{
  public:
    // `file` is where the code is saved; none for a plugin not saved yet.
    PluginWorkbench(olagarro::oui::Application& application, std::string source,
                    std::optional<std::filesystem::path> file);
    ~PluginWorkbench() override;
    PluginWorkbench(const PluginWorkbench&) = delete;
    PluginWorkbench& operator=(const PluginWorkbench&) = delete;

    [[nodiscard]] olagarro::oui::TextEdit& code() { return *m_code; }
    [[nodiscard]] const std::optional<std::filesystem::path>& file() const { return m_file; }
    // The code is in `file` as it is now.
    void saved(std::filesystem::path file);
    [[nodiscard]] bool modified() const;
    // The tab's title: the file's name, or "New Plugin", with " *" when changed since saved.
    [[nodiscard]] std::string title() const;

    // The open images it can try the effect on, by name, and how to have one.
    void set_open_images(std::vector<std::string> names, std::function<std::optional<Image>(std::size_t)> image);
    // An image from a file to try the effect on, which the image list then offers and shows.
    void set_loaded_image(std::string name, Image image);
    // Load Image was pressed: the owner asks for a file and gives it with set_loaded_image.
    void on_load_image(std::function<void()> load) { m_load_image = std::move(load); }
    // The title changed, as when the code is first changed after saving.
    void on_title_changed(std::function<void()> changed) { m_title_changed = std::move(changed); }

    // What the last run came to, once it has.
    [[nodiscard]] const std::optional<PluginTrial>& last_trial() const { return m_trial; }
    // Runs it now, as it does a moment after a change.
    void run_now();
    // After each event the application handled: what was typed may open the suggestions of what the code can use,
    // and the cursor in the brackets of a call shows how it is called.
    void after_event(const olagarro::oui::Event& event);
    // Opens the suggestions where the code's cursor is, as Ctrl+Space asks.
    void suggest();

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;

  private:
    // Runs it again a moment from now, unless something changes in between.
    void schedule();
    void start_run();
    void show(PluginTrial trial);
    void show_image(const Image& image);
    void rebuild_controls(const Effect& effect);
    void refresh_image_choices();
    [[nodiscard]] Image chosen_image() const;

    olagarro::oui::Application& m_application;
    std::optional<std::filesystem::path> m_file;
    std::string m_saved_source;
    olagarro::oui::TextEdit* m_code{};
    olagarro::oui::ComboBox* m_effects{};
    olagarro::oui::ComboBox* m_images{};
    olagarro::oui::Container* m_preview_holder{};
    olagarro::oui::Grid* m_controls{};
    olagarro::oui::TextEdit* m_log{};
    olagarro::oui::Button* m_error{};
    std::unique_ptr<OpenDocument> m_preview;
    CanvasView* m_preview_view{};

    // What it is tried on: the test card, an open image, or the one loaded.
    std::vector<std::string> m_open_names;
    std::function<std::optional<Image>(std::size_t)> m_open_image;
    std::optional<std::pair<std::string, Image>> m_loaded;
    // The open image chosen, by name, so it stays chosen as others open and close.
    std::string m_chosen_name;
    Image m_test_card;
    Image m_tried_on;

    // The effect shown, by name, and the values of each effect's parameters, by effect and id.
    std::string m_effect_name;
    std::map<std::string, std::map<std::string, double>> m_values;
    // The parameters the controls show, to build them again only when they change.
    std::vector<EffectParameter> m_shown_parameters;
    std::optional<PluginTrial> m_trial;
    std::optional<std::size_t> m_error_line;

    olagarro::oui::Timer m_pending;
    // One run at a time, on a thread of its own; a change while it runs runs it again after.
    std::thread m_worker;
    bool m_running{false};
    bool m_again{false};
    // Gone once the workbench is, for what a run posts after.
    std::shared_ptr<int> m_alive{std::make_shared<int>(0)};
    std::unique_ptr<olagarro::oui::Suggester> m_suggester;
    // What the code can use: paint.d.ts with the values of the effect shown, made again when its parameters change.
    std::unique_ptr<olagarro::oui::ScriptApi> m_api;
    std::function<void()> m_load_image;
    std::function<void()> m_title_changed;
    bool m_was_modified{false};
};

// Colours JavaScript, as the workbench shows a plugin's code.
[[nodiscard]] std::shared_ptr<const olagarro::oui::SyntaxHighlighter> javascript_highlighter();

} // namespace oui_paint

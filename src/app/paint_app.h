#pragma once

#include "app/new_image.h"
#include "app/open_document.h"
#include "core/selection.h"
#include "tools/stroke.h"
#include "ui/icons.h"

#include <oui/application.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/settings.h>
#include <oui/tab_view.h>

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace oui_paint
{

class CanvasView;
class ColorPanel;
class LayersPanel;
class MainFrame;
class ToolOptions;
class ToolPanel;
struct ToolEvent;

// OUI Paint as a whole: its window, its open documents in tabs, and the commands that work on them.
class PaintApp
{
  public:
    // `settings` keeps what the person chose between sessions, such as the interface's size, which `window` has.
    PaintApp(olagarro::oui::Application& application, olagarro::oui::PixelBackend& window,
             olagarro::oui::Settings& settings);
    PaintApp(const PaintApp&) = delete;
    PaintApp& operator=(const PaintApp&) = delete;

    // Opens a file in a new tab; false, having said why, when it cannot.
    bool open(const std::filesystem::path& path);
    // A new tab holding `document`.
    void add(std::unique_ptr<OpenDocument> document);
    [[nodiscard]] std::size_t document_count() const { return m_documents.size(); }
    [[nodiscard]] OpenDocument* current();

    void new_image();
    void open_with_dialog();
    // Saves the current document to its file, or asks for one first; false when it was not saved.
    bool save(bool ask_for_path);
    // Closes a tab, asking first about unsaved changes; false when the person kept it open.
    bool close(std::size_t index);
    // Saves every document with unsaved changes, asking for a file for those that have none.
    void save_all();
    // Closes every tab and quits, unless the person keeps one open.
    void quit();

  private:
    void register_commands();
    // What the tool in use does with the pointer on the current document.
    void use_tool(const ToolEvent& event);
    // Changes the current document's layers as one undoable step called `name`, and draws again what changed.
    void change_layers(std::string name, const std::function<void(Document&)>& change);
    void layer_properties();
    // Shows the current document's layers in the panel.
    void show_layers();
    // Uses the rectangle or ellipse select tool.
    void select_shape(const ToolEvent& event, bool ellipse);
    // Starts or stops the marching ants as the current document has a selection or not.
    void update_ants();
    void undo();
    void redo();
    // Asks for the primary or the secondary colour as hexadecimal.
    void edit_color(bool primary);
    void build_window();
    [[nodiscard]] CanvasView* current_view();
    void refresh_titles();
    void show_error(std::string_view title, std::string_view message);
    // Writes `document` to `path` in the format its extension names; false, having said why, when it did not.
    bool write(OpenDocument& document, const std::filesystem::path& path);

    olagarro::oui::Application& m_application;
    olagarro::oui::PixelBackend& m_window;
    olagarro::oui::Settings& m_settings;
    Icons m_icons;
    MainFrame* m_frame{};
    ToolPanel* m_tool_panel{};
    ColorPanel* m_color_panel{};
    LayersPanel* m_layers_panel{};
    ToolOptions* m_tool_options{};
    std::unique_ptr<Stroke> m_stroke;
    // A rectangle or ellipse being drawn to select: where it began, how it combines, and the selection before it.
    struct Selecting
    {
        Position from;
        SelectionMode mode;
        std::optional<Layer> before;
    };
    std::optional<Selecting> m_selecting;
    // The marching ants move while there is a selection to show.
    olagarro::oui::Timer m_ants;
    int m_ants_phase{0};
    olagarro::oui::TabView* m_tabs{};
    std::vector<std::unique_ptr<OpenDocument>> m_documents;
    NewImage m_last_new_image;
    int m_untitled_count{0};
    std::filesystem::path m_last_folder;
};

} // namespace oui_paint

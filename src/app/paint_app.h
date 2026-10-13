#pragma once

#include "app/new_image.h"
#include "app/open_document.h"
#include "tools/stroke.h"
#include "ui/icons.h"

#include <oui/application.h>
#include <oui/tab_view.h>

#include <filesystem>
#include <memory>
#include <vector>

namespace oui_paint
{

class CanvasView;
class ColorPanel;
class MainFrame;
class ToolOptions;
class ToolPanel;
struct ToolEvent;

// OUI Paint as a whole: its window, its open documents in tabs, and the commands that work on them.
class PaintApp
{
  public:
    explicit PaintApp(olagarro::oui::Application& application);
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
    Icons m_icons;
    MainFrame* m_frame{};
    ToolPanel* m_tool_panel{};
    ColorPanel* m_color_panel{};
    ToolOptions* m_tool_options{};
    std::unique_ptr<Stroke> m_stroke;
    olagarro::oui::TabView* m_tabs{};
    std::vector<std::unique_ptr<OpenDocument>> m_documents;
    NewImage m_last_new_image;
    int m_untitled_count{0};
    std::filesystem::path m_last_folder;
};

} // namespace oui_paint

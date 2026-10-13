#pragma once

#include "app/new_image.h"
#include "app/open_document.h"
#include "core/selection.h"
#include "effects/plugins.h"
#include "tools/floating.h"
#include "tools/stroke.h"
#include "ui/canvas_view.h"
#include "ui/icons.h"

#include <oui/application.h>
#include <oui/menu_bar.h>
#include <oui/graphics/font.h>
#include <oui/graphics/pixel_backend.h>
#include <oui/script/system_api.h>
#include <oui/settings.h>
#include <oui/tab_view.h>

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace oui_paint
{

class CanvasView;
class ColorPanel;
class LayersPanel;
class PalettesPanel;
class MainFrame;
class ToolOptions;
class ToolPanel;

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
    // Loads the plugins built in and those in the plugins folder beside the settings, and puts their effects in the
    // Adjustments and Effects menus.
    void load_plugins();
    // Runs an effect on the active layer, inside the selection: asking for its parameters, showing it as they change,
    // unless `again`, which runs it with the values it had last.
    void run_effect(std::size_t index, bool again);
    // Tells the plugins' hooks a file was saved, and what went wrong in them, if anything.
    void tell_saved(const std::filesystem::path& path);
    // What the tool in use does with the pointer on the current document.
    void use_tool(const ToolEvent& event);
    // Hands a tool what the pointer did, keeping only the last move of a frame for tools that do not trace it.
    void take_tool_event(const ToolEvent& event);
    void use_pending_move();
    // Changes the current document's layers as one undoable step called `name`, and draws again what changed.
    void change_layers(std::string name, const std::function<void(Document&)>& change);
    void layer_properties();
    // Shows the current document's layers in the panel.
    void show_layers();
    // Makes `tool` the one in use, putting down what floats and showing its options.
    void select_tool(const std::string& tool);
    // Uses a tool drawn by dragging, gradient, line or shape, drawn anew from the layer as it was at each move.
    void drag_draw(const ToolEvent& event, const std::string& tool);
    // Uses the rectangle or ellipse select tool.
    void select_shape(const ToolEvent& event, bool ellipse);
    // Copies what is selected of the active layer, or of the whole image when `merged`, as PNG.
    bool copy(bool merged);
    // The image the clipboard holds, in any format OUI Paint reads; none when it holds none.
    [[nodiscard]] std::optional<Image> clipboard_image();
    enum class PasteInto
    {
        layer,
        new_layer,
        new_image,
    };
    void paste(PasteInto into);
    // Uses Move Selected Pixels, or Move Selection when `outline_only`.
    void move_selected(const ToolEvent& event, bool outline_only);
    // What the Text tool does with a key: true when it took it.
    bool type_key(const olagarro::oui::Event& event);
    // Draws the text being typed, with its cursor while it is being typed.
    void show_typing(bool with_cursor);
    [[nodiscard]] olagarro::oui::Font* text_font();
    // Lifts what is selected into a floating piece unless one is floating already.
    void ensure_floating();
    // Lays the floating piece as its transform says, and draws again what changed.
    void place_floating();
    // Turns or mirrors what is selected, as one undoable step.
    void transform_selection(std::string name, const std::function<void(PieceTransform&)>& change);
    // The floating piece's eight handles in the view: the corners, then the middles of the edges.
    [[nodiscard]] std::vector<Position> floating_handles();
    // Puts floating pixels down where they are, and text being typed.
    void settle();
    // Starts or stops the marching ants as the current document has a selection or not.
    void update_ants();
    void undo();
    void redo();
    // What a tool paints with: the colour, or in an indexed image the index as its pixel's grey, and whether that
    // index is the see-through one of the active layer's palette.
    [[nodiscard]] Color ink(bool secondary, bool* see_through = nullptr);
    // Changes the current document's palettes as one undoable step.
    void change_palettes(std::string name, const std::function<void(PaletteSetup&)>& change);
    // The Palettes menu's commands, and turning an image indexed or into colours.
    void register_palette_commands();
    void convert_to_indexed();
    // Asks for the primary or the secondary colour as hexadecimal.
    void edit_color(bool primary);
    void build_window();
    [[nodiscard]] CanvasView* current_view();
    void refresh_titles();
    void show_error(std::string_view title, std::string_view message);
    // Puts a file first among the recent ones, kept in the settings.
    void remember(const std::filesystem::path& path);
    // The Open Recent menu's entries, made each time it opens.
    [[nodiscard]] std::vector<olagarro::oui::MenuItem> recent_items();
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
    PalettesPanel* m_palettes_panel{};
    // In an indexed image, the indices picked as primary and secondary; none when a colour was picked, whose nearest
    // index of the layer's palette is used.
    std::optional<int> m_primary_index;
    std::optional<int> m_secondary_index;
    ToolOptions* m_tool_options{};
    std::unique_ptr<Stroke> m_stroke;
    std::unique_ptr<PluginHost> m_plugins;
    std::unique_ptr<olagarro::oui::script::SystemApi> m_system;
    // Each effect's values as last used, and the effect run last, for Ctrl+F.
    std::map<std::size_t, std::map<std::string, double>> m_effect_values;
    std::optional<std::size_t> m_last_effect;
    // A rectangle or ellipse being drawn to select: where it began, how it combines, and the selection before it.
    struct Selecting
    {
        Position from;
        SelectionMode mode;
        std::optional<Layer> before;
    };
    std::optional<Selecting> m_selecting;
    // The points of a lasso being drawn.
    std::vector<Position> m_lasso;
    // Pixels lifted by Move Selected Pixels, floating until something else is done: the layer with the hole they left,
    // the selection where they were, what they are, and how far they have been moved.
    struct Floating
    {
        OpenDocument* document{};
        // The layer with the hole the piece left, what it is laid over.
        Layer base;
        FloatingPiece piece;
        PieceTransform transform;
    };
    std::optional<Floating> m_floating;
    // A move being dragged: how, where it was pressed, and the transform it started from.
    struct Moving
    {
        enum class Kind
        {
            move,
            rotate,
            // Pulling one of the eight handles: 0 to 3 the corners from the top left round, 4 to 7 the edges from the
            // top round.
            scale,
            outline,
        };
        Kind kind{Kind::move};
        Position from;
        PieceTransform start;
        int handle{0};
        std::optional<Layer> selection;
    };
    std::optional<Moving> m_moving;
    // A gradient, line or shape being dragged: where it began, the layer before it, and which button.
    struct Dragging
    {
        Position from;
        Layer base;
        bool secondary{false};
    };
    std::optional<Dragging> m_dragging;
    // Text being typed with the Text tool: where, what, and the layer before it.
    struct Typing
    {
        OpenDocument* document{};
        Position at;
        std::string text;
        Layer base;
    };
    std::optional<Typing> m_typing;
    // Fonts as loaded, by file and size; the bytes must outlive the fonts made of them.
    std::map<std::filesystem::path, std::string> m_font_files;
    std::map<std::pair<std::filesystem::path, int>, std::unique_ptr<olagarro::oui::Font>> m_fonts;
    // Where the Clone Stamp copies from, set with Ctrl and a click, and how far that is from where it paints, fixed by
    // its first stroke.
    std::optional<Position> m_clone_source;
    std::optional<std::pair<int, int>> m_clone_offset;
    // Where the Pan tool's drag was last, in the view.
    std::optional<Position> m_panning;
    // A drag's latest move, waiting for the frame to end so the moves before it in the same frame are skipped.
    std::optional<ToolEvent> m_pending_move;
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

#include "app/paint_app.h"

#include "app/tools.h"
#include "codecs/codecs.h"
#include "codecs/openraster.h"
#include "ui/canvas_view.h"
#include "ui/color_panel.h"
#include "ui/layer_properties_dialog.h"
#include "ui/layers_panel.h"
#include "ui/main_frame.h"
#include "ui/new_image_dialog.h"
#include "ui/tool_options.h"
#include "ui/tool_panel.h"

#include <oui/dialogs.h>
#include <oui/file_dialog.h>
#include <oui/layout.h>
#include <oui/window.h>

#include <cmath>
#include <cstdio>
#include <string>

using namespace olagarro;

namespace oui_paint
{

namespace
{

const std::vector<oui::FileFilter>& image_filters()
{
    static const std::vector<oui::FileFilter> filters{
        {"Images (*.ora, *.png, *.jpg, *.bmp, *.tga, *.gif, *.r16)",
         "*.ora *.png *.jpg *.jpeg *.bmp *.tga *.gif *.r16 *.raw"},
        {"OpenRaster, with layers (*.ora)", "*.ora"},
        {"PNG (*.png)", "*.png"},
        {"JPEG (*.jpg)", "*.jpg *.jpeg"},
        {"BMP (*.bmp)", "*.bmp"},
        {"TGA (*.tga)", "*.tga"},
        {"Raw 16-bit heightmap (*.r16)", "*.r16 *.raw"},
    };
    return filters;
}

std::vector<ToolButton> tool_buttons()
{
    std::vector<ToolButton> buttons;
    for(const auto& tool : all_tools())
    {
        const auto key{static_cast<char>(tool.key - U'a' + U'A')};
        buttons.push_back({tool.id, tool.icon, tool.title + " (" + std::string(1, key) + ")"});
    }
    return buttons;
}

} // namespace

PaintApp::PaintApp(oui::Application& application, oui::PixelBackend& window, oui::Settings& settings)
    : m_application{application}, m_window{window}, m_settings{settings}
{
    std::error_code ignored;
    m_last_folder = std::filesystem::current_path(ignored);
    register_commands();
    build_window();
    m_application.on_close_requested([this] { quit(); });
    // The interface's size, kept as it was left.
    m_application.on_zoomed(
        [this]
        {
            m_settings.set_int_value("interface_size", m_window.zoom());
            static_cast<void>(m_settings.save());
        });
}

void PaintApp::register_commands()
{
    auto& commands{m_application.commands()};
    // Ctrl with plus, minus and 0 zoom the image, as in Paint.NET, not the interface.
    for(const auto* const id : {"zoom.in", "zoom.out", "zoom.reset"})
    {
        commands.unregister_command(id);
    }
    const auto add{[&commands](std::string_view id, std::string_view title, auto&& run, oui::Shortcut shortcut)
                   {
                       commands.register_command(id, title, std::forward<decltype(run)>(run));
                       commands.bind(id, shortcut);
                   }};
    add("file.new", "&New...", [this] { new_image(); }, {U'n', true});
    add("file.open", "&Open...", [this] { open_with_dialog(); }, {U'o', true});
    add("file.save", "&Save", [this] { static_cast<void>(save(false)); }, {U's', true});
    add("file.save_as", "Save &As...", [this] { static_cast<void>(save(true)); }, {U's', true, false, true});
    add("file.save_all", "Save A&ll", [this] { save_all(); }, {U's', true, true});
    add("file.close", "&Close", [this] { static_cast<void>(close(m_tabs->selected_index())); }, {U'w', true});
    commands.bind("file.close", oui::KeyShortcut{oui::Key::f4, true});
    commands.register_command("file.quit", "E&xit", [this] { quit(); });
    commands.bind("file.quit", oui::KeyShortcut{oui::Key::f4, false, true});
    const auto switch_tab{[this](int step)
                          {
                              return [this, step]
                              {
                                  const auto count{static_cast<int>(m_documents.size())};
                                  if(count > 1)
                                  {
                                      const auto index{(static_cast<int>(m_tabs->selected_index()) + step + count) %
                                                       count};
                                      m_tabs->select(static_cast<std::size_t>(index));
                                  }
                              };
                          }};
    commands.register_command("window.next_image", "&Next Image", switch_tab(1));
    commands.bind("window.next_image", oui::KeyShortcut{oui::Key::tab, true});
    commands.bind("window.next_image", oui::KeyShortcut{oui::Key::page_down, true});
    commands.register_command("window.previous_image", "&Previous Image", switch_tab(-1));
    commands.bind("window.previous_image", oui::KeyShortcut{oui::Key::shift_tab, true});
    commands.bind("window.previous_image", oui::KeyShortcut{oui::Key::page_up, true});
    for(int number{1}; number <= 9; ++ number)
    {
        const auto id{"window.image_" + std::to_string(number)};
        commands.register_command(id, "Image " + std::to_string(number),
                                  [this, number]
                                  {
                                      if(static_cast<std::size_t>(number) <= m_documents.size())
                                      {
                                          m_tabs->select(static_cast<std::size_t>(number - 1));
                                      }
                                  });
        commands.bind(id, {static_cast<char32_t>(U'0' + number), false, true});
    }
    add("edit.undo", "&Undo", [this] { undo(); }, {U'z', true});
    add("edit.redo", "&Redo", [this] { redo(); }, {U'y', true});
    add("colors.swap", "Swap Colors", [this] { m_color_panel->swap(); }, {U'x'});
    add("edit.select_all", "Select &All",
        [this] { change_layers("Select All", [](Document& document) { document.selection().select_all(); }); },
        {U'a', true});
    add("edit.deselect", "&Deselect",
        [this] { change_layers("Deselect", [](Document& document) { document.selection().clear(); }); }, {U'd', true});
    add("edit.invert_selection", "&Invert Selection",
        [this] { change_layers("Invert Selection", [](Document& document) { document.selection().invert(); }); },
        {U'i', true});
    commands.register_command("edit.erase_selection", "&Erase Selection",
                              [this]
                              {
                                  change_layers("Erase Selection",
                                                [](Document& document)
                                                {
                                                    static_cast<void>(erase_selected(
                                                        document.layers()[document.active_layer()],
                                                        document.selection()));
                                                });
                              });
    commands.bind("edit.erase_selection", oui::KeyShortcut{oui::Key::delete_});
    commands.register_command("edit.fill_selection", "&Fill Selection",
                              [this]
                              {
                                  const auto color{m_color_panel->primary()};
                                  change_layers("Fill Selection",
                                                [&color](Document& document)
                                                {
                                                    static_cast<void>(fill_selected(
                                                        document.layers()[document.active_layer()],
                                                        document.selection(), color));
                                                });
                              });
    commands.bind("edit.fill_selection", oui::KeyShortcut{oui::Key::backspace});
    add("brush.narrower", "Brush Width Down", [this] { m_tool_options->change_width(-1); }, {U'['});
    add("brush.wider", "Brush Width Up", [this] { m_tool_options->change_width(1); }, {U']'});
    add("brush.much_narrower", "Brush Width Down by 5", [this] { m_tool_options->change_width(-5); }, {U'[', true});
    add("brush.much_wider", "Brush Width Up by 5", [this] { m_tool_options->change_width(5); }, {U']', true});
    const auto active{[this] { return current() != nullptr ? current()->document().active_layer() : 0u; }};
    add("layers.add", "&Add New Layer",
        [this]
        {
            change_layers("Add New Layer",
                          [](Document& document)
                          { document.insert_layer_above("Layer " + std::to_string(document.layers().size() + 1)); });
        },
        {U'n', true, false, true});
    add("layers.delete", "&Delete Layer",
        [this, active] { change_layers("Delete Layer", [&](Document& document) { document.remove_layer(active()); }); },
        oui::Shortcut{0, true, false, true, oui::Key::delete_});
    add("layers.duplicate", "D&uplicate Layer",
        [this, active]
        { change_layers("Duplicate Layer", [&](Document& document) { document.duplicate_layer(active()); }); },
        {U'd', true, false, true});
    add("layers.merge_down", "&Merge Layer Down",
        [this, active] { change_layers("Merge Layer Down", [&](Document& document) { document.merge_down(active()); }); },
        {U'm', true});
    commands.register_command("layers.move_up", "Move Layer &Up",
                              [this, active]
                              {
                                  change_layers("Move Layer Up",
                                                [&](Document& document)
                                                {
                                                    const auto index{active()};
                                                    if(index + 1 < document.layers().size())
                                                    {
                                                        document.move_layer(index, index + 1);
                                                    }
                                                });
                              });
    commands.register_command("layers.move_down", "Move Layer D&own",
                              [this, active]
                              {
                                  change_layers("Move Layer Down",
                                                [&](Document& document)
                                                {
                                                    const auto index{active()};
                                                    if(index > 0)
                                                    {
                                                        document.move_layer(index, index - 1);
                                                    }
                                                });
                              });
    add("layers.toggle_visible", "Show or &Hide Layer",
        [this, active]
        {
            change_layers("Layer Visibility",
                          [&](Document& document)
                          {
                              auto& layer{document.layers()[active()]};
                              layer.set_visible(!layer.visible());
                          });
        },
        {U',', true});
    add("layers.properties", "Layer &Properties...", [this] { layer_properties(); },
        oui::Shortcut{0, false, false, false, oui::Key::f4});
    const auto select_layer{[this](int step, bool to_end)
                            {
                                return [this, step, to_end]
                                {
                                    auto* const open{current()};
                                    if(open == nullptr)
                                    {
                                        return;
                                    }
                                    auto& document{open->document()};
                                    const auto last{static_cast<int>(document.layers().size()) - 1};
                                    const auto index{to_end ? (step > 0 ? last : 0)
                                                            : std::clamp(static_cast<int>(document.active_layer()) + step,
                                                                         0, last)};
                                    document.set_active_layer(static_cast<std::size_t>(index));
                                };
                            }};
    commands.register_command("layers.select_above", "Layer Above", select_layer(1, false));
    commands.bind("layers.select_above", oui::KeyShortcut{oui::Key::page_up, false, true});
    commands.register_command("layers.select_below", "Layer Below", select_layer(-1, false));
    commands.bind("layers.select_below", oui::KeyShortcut{oui::Key::page_down, false, true});
    commands.register_command("layers.select_top", "Top Layer", select_layer(1, true));
    commands.bind("layers.select_top", oui::KeyShortcut{oui::Key::page_up, true, true});
    commands.register_command("layers.select_bottom", "Bottom Layer", select_layer(-1, true));
    commands.bind("layers.select_bottom", oui::KeyShortcut{oui::Key::page_down, true, true});
    add("image.flatten", "&Flatten",
        [this] { change_layers("Flatten", [](Document& document) { document.flatten_layers(); }); },
        {U'f', true, false, true});
    // Each tool's letter, and again for the next tool with the same letter.
    for(const auto& tool : all_tools())
    {
        const auto id{std::string{"tool.key_"} + static_cast<char>(tool.key)};
        if(commands.shortcut(id))
        {
            continue;
        }
        commands.register_command(id, tool.title,
                                  [this, key{tool.key}]
                                  {
                                      if(const auto next{tool_for_key(key, m_tool_panel->selected())})
                                      {
                                          settle();
                                          m_tool_panel->set_selected(*next);
                                      }
                                  });
        commands.bind(id, {tool.key});
    }
    const auto view{[this](void (CanvasView::*action)())
                    {
                        return [this, action]
                        {
                            if(auto* const canvas{current_view()})
                            {
                                (canvas->*action)();
                            }
                        };
                    }};
    add("view.zoom_in", "Zoom &In", view(&CanvasView::zoom_in), {U'+', true});
    add("view.zoom_out", "Zoom &Out", view(&CanvasView::zoom_out), {U'-', true});
    add("view.zoom_to_window", "Zoom to &Window", view(&CanvasView::zoom_to_fit), {U'b', true});
    add("view.actual_size", "&Actual Size", view(&CanvasView::actual_size), {U'0', true});
    // The interface's text, bigger or smaller: Ctrl with plus and minus zoom the image, so Ctrl and Alt with them.
    add("view.interface_bigger", "&Bigger Interface", [this] { static_cast<void>(m_application.zoom(1)); },
        {U'=', true, true});
    commands.bind("view.interface_bigger", {U'+', true, true});
    add("view.interface_smaller", "&Smaller Interface", [this] { static_cast<void>(m_application.zoom(-1)); },
        {U'-', true, true});
    add("view.interface_reset", "&Interface Size as It Starts", [this] { static_cast<void>(m_application.zoom(0)); },
        {U'0', true, true});
    // Ctrl and = zooms in too, as plus is Shift and = on many keyboards.
    commands.register_command("view.zoom_in_equals", "Zoom In", view(&CanvasView::zoom_in));
    commands.bind("view.zoom_in_equals", {U'=', true});
}

void PaintApp::build_window()
{
    auto frame{std::make_unique<MainFrame>(m_application.commands())};
    m_frame = frame.get();
    m_frame->menu_bar().add_menu("&File", {"file.new", "file.open", oui::menu_separator(), "file.save",
                                           "file.save_as", "file.save_all", oui::menu_separator(), "file.close",
                                           "file.quit"});
    m_frame->menu_bar().add_menu("&Edit", {"edit.undo", "edit.redo", oui::menu_separator(), "edit.erase_selection",
                                           "edit.fill_selection", oui::menu_separator(), "edit.select_all",
                                           "edit.deselect", "edit.invert_selection"});
    m_frame->menu_bar().add_menu("&Image", {"image.flatten"});
    m_frame->menu_bar().add_menu("&Layers", {"layers.add", "layers.delete", "layers.duplicate", "layers.merge_down",
                                             oui::menu_separator(), "layers.move_up", "layers.move_down",
                                             "layers.toggle_visible", oui::menu_separator(), "layers.properties"});
    m_frame->menu_bar().add_menu("&View", {"view.zoom_in", "view.zoom_out", "view.zoom_to_window",
                                           "view.actual_size", oui::menu_separator(), "view.interface_bigger",
                                           "view.interface_smaller", "view.interface_reset"});
    auto left{oui::column()};
    m_tool_panel = &left->add(std::make_unique<ToolPanel>(tool_buttons(), m_icons));
    m_tool_panel->set_selected("brush");
    m_tool_panel->on_picked([this](const std::string&) { settle(); });
    m_color_panel = &left->add(std::make_unique<ColorPanel>());
    m_color_panel->on_edit([this](bool primary) { edit_color(primary); });
    m_frame->set_left(std::move(left));
    auto layers{std::make_unique<LayersPanel>(m_icons)};
    m_layers_panel = layers.get();
    m_layers_panel->on_select(
        [this](std::size_t index)
        {
            if(auto* const open{current()})
            {
                open->document().set_active_layer(index);
            }
        });
    m_layers_panel->on_toggle_visible(
        [this](std::size_t index)
        {
            change_layers("Layer Visibility",
                          [index](Document& document)
                          {
                              auto& layer{document.layers()[index]};
                              layer.set_visible(!layer.visible());
                          });
        });
    m_layers_panel->on_action(
        [this](LayersPanel::Action action)
        {
            const auto run{[this](const char* id) { static_cast<void>(m_application.commands().execute(id)); }};
            switch(action)
            {
            case LayersPanel::Action::add:
                run("layers.add");
                break;
            case LayersPanel::Action::remove:
                run("layers.delete");
                break;
            case LayersPanel::Action::duplicate:
                run("layers.duplicate");
                break;
            case LayersPanel::Action::merge_down:
                run("layers.merge_down");
                break;
            case LayersPanel::Action::move_up:
                run("layers.move_up");
                break;
            case LayersPanel::Action::move_down:
                run("layers.move_down");
                break;
            case LayersPanel::Action::properties:
                run("layers.properties");
                break;
            }
        });
    m_frame->set_right(std::move(layers));
    auto options{std::make_unique<ToolOptions>(BrushSettings{})};
    m_tool_options = options.get();
    m_frame->set_top(std::move(options));
    auto tabs{std::make_unique<oui::TabView>()};
    m_tabs = tabs.get();
    m_tabs->set_highlight_predicate([this](std::size_t index)
                                    { return index < m_documents.size() && m_documents[index]->modified(); });
    static_cast<void>(m_tabs->on_close_requested([this](std::size_t index) { static_cast<void>(close(index)); }));
    static_cast<void>(m_tabs->on_selection_changed([this](auto&&...) { show_layers(); }));
    m_frame->set_center(std::move(tabs));
    m_frame->status_bar().set_provider(
        [this]
        {
            oui::StatusText text;
            auto* const view{current_view()};
            if(view == nullptr)
            {
                return text;
            }
            if(const auto& pointer{view->pointer()})
            {
                text.left = std::to_string(static_cast<int>(pointer->x)) + ", " +
                            std::to_string(static_cast<int>(pointer->y));
            }
            const auto& document{view->document().document()};
            char zoom[32];
            std::snprintf(zoom, sizeof zoom, "%g%%", std::round(view->document().viewport().zoom() * 1000.0) / 10.0);
            text.right = std::to_string(document.width()) + " x " + std::to_string(document.height()) + "   " +
                         std::string{describe(document.format())} + "   " + zoom;
            return text;
        });

    auto window{oui::window("OUI Paint")};
    window->set_border(oui::WindowBorder::none);
    window->set_close_on_escape(false);
    window->add(std::move(frame));
    m_application.set_window(std::move(window));
}

OpenDocument* PaintApp::current()
{
    if(m_documents.empty())
    {
        return nullptr;
    }
    return m_documents[m_tabs->selected_index()].get();
}

CanvasView* PaintApp::current_view()
{
    return m_tabs == nullptr ? nullptr : m_tabs->selected_content<CanvasView>();
}

void PaintApp::refresh_titles()
{
    for(std::size_t index{}; index < m_documents.size(); ++ index)
    {
        const auto& document{*m_documents[index]};
        m_tabs->set_title(index, document.modified() ? document.title() + " *" : document.title());
    }
}

void PaintApp::add(std::unique_ptr<OpenDocument> document)
{
    auto view{std::make_unique<CanvasView>(*document)};
    view->on_tool([this](const ToolEvent& event) { use_tool(event); });
    m_documents.push_back(std::move(document));
    auto& added{m_tabs->add_tab(m_documents.back()->title(), std::move(view))};
    m_tabs->select(m_documents.size() - 1);
    m_application.focus(added);
    refresh_titles();
    show_layers();
}

void PaintApp::new_image()
{
    auto start{m_last_new_image};
    const auto request{ask_new_image(m_application, start)};
    if(!request)
    {
        return;
    }
    m_last_new_image = *request;
    auto document{std::make_unique<OpenDocument>(make_document(*request))};
    document->set_untitled_number(++ m_untitled_count);
    add(std::move(document));
}

void PaintApp::show_error(std::string_view title, std::string_view message)
{
    static_cast<void>(oui::message_box(m_application, title, message));
}

bool PaintApp::open(const std::filesystem::path& path)
{
    std::string error;
    const auto bytes{read_file(path, error)};
    if(!bytes)
    {
        show_error("Open", error);
        return false;
    }
    if(file_format_of(path.string()) == FileFormat::openraster)
    {
        auto layered{read_openraster(*bytes, error)};
        if(!layered)
        {
            show_error("Open", path.filename().string() + ": " + error);
            return false;
        }
        auto document{std::make_unique<OpenDocument>(std::move(*layered))};
        document->set_path(path);
        m_last_folder = path.parent_path();
        add(std::move(document));
        return true;
    }
    std::optional<Image> image;
    if(file_format_of(path.string()) == FileFormat::raw16)
    {
        const auto guess{guess_raw16_size(bytes->size())};
        oui::PromptOptions options;
        options.title = "Open Raw Heightmap";
        options.label = "Size, as width x height:";
        options.text = guess ? std::to_string(guess->first) + " x " + std::to_string(guess->second) : "";
        options.validate = [&bytes](std::string_view text) -> std::optional<std::string>
        {
            int width{};
            int height{};
            if(std::sscanf(std::string{text}.c_str(), "%d x %d", &width, &height) != 2 || width <= 0 || height <= 0 ||
               static_cast<std::size_t>(width) * height * 2u != bytes->size())
            {
                return "The file holds " + std::to_string(bytes->size() / 2) + " values.";
            }
            return std::nullopt;
        };
        const auto size{oui::prompt(m_application, options)};
        if(!size)
        {
            return false;
        }
        int width{};
        int height{};
        static_cast<void>(std::sscanf(size->c_str(), "%d x %d", &width, &height));
        image = read_raw16(*bytes, width, height);
    }
    else
    {
        image = read_image(*bytes, error);
    }
    if(!image)
    {
        show_error("Open", path.filename().string() + ": " + error);
        return false;
    }
    auto document{std::make_unique<OpenDocument>(Document::from_image(*image))};
    document->set_path(path);
    m_last_folder = path.parent_path();
    add(std::move(document));
    return true;
}

void PaintApp::open_with_dialog()
{
    oui::FileDialogOptions options;
    options.mode = oui::FileDialogMode::open_file;
    options.directory = m_last_folder;
    options.filters = image_filters();
    if(const auto path{oui::file_dialog(m_application, options)})
    {
        static_cast<void>(open(*path));
    }
}

bool PaintApp::write(OpenDocument& document, const std::filesystem::path& path)
{
    const auto format{file_format_of(path.string())};
    if(!format || !can_write(*format))
    {
        show_error("Save", "OUI Paint cannot write " + path.filename().string() +
                               ": name it .ora, .png, .jpg, .bmp, .tga or .r16.");
        return false;
    }
    if(*format == FileFormat::openraster)
    {
        std::string error;
        try
        {
            if(!write_file_atomically(path, write_openraster(document.document()), error))
            {
                show_error("Save", error);
                return false;
            }
        }
        catch(const std::exception& failure)
        {
            show_error("Save", failure.what());
            return false;
        }
        document.set_path(path);
        document.set_modified(false);
        m_last_folder = path.parent_path();
        refresh_titles();
        return true;
    }
    if(document.document().layers().size() > 1)
    {
        const auto answer{oui::message_box(m_application, "Save",
                                           "This file holds one layer: the image is saved flattened.\n"
                                           "Its layers stay here, and in an .ora file if it is saved as one.",
                                           {"&Flatten", "Cancel"})};
        if(answer != 0u)
        {
            return false;
        }
    }
    const auto pixels{document.document().format()};
    const auto target{written_format(*format, pixels)};
    if(target != pixels)
    {
        const auto answer{oui::message_box(m_application, "Save",
                                           "The image is " + std::string{describe(pixels)} + ".\nThis file holds " +
                                               std::string{describe(target)} + ": save it reduced?",
                                           {"&Reduce", "Cancel"})};
        if(answer != 0u)
        {
            return false;
        }
    }
    std::string error;
    try
    {
        const auto bytes{write_image(converted(document.document().flatten(), target), *format)};
        if(!write_file_atomically(path, bytes, error))
        {
            show_error("Save", error);
            return false;
        }
    }
    catch(const std::exception& failure)
    {
        show_error("Save", failure.what());
        return false;
    }
    document.set_path(path);
    document.set_modified(false);
    m_last_folder = path.parent_path();
    refresh_titles();
    return true;
}

bool PaintApp::save(bool ask_for_path)
{
    auto* const document{current()};
    if(document == nullptr)
    {
        return false;
    }
    auto path{document->path()};
    if(ask_for_path || !path)
    {
        oui::FileDialogOptions options;
        options.mode = oui::FileDialogMode::save_file;
        options.directory = path ? path->parent_path() : m_last_folder;
        options.file_name = path ? path->filename().string()
                                 : document->title() + (document->document().layers().size() > 1 ? ".ora" : ".png");
        options.filters = image_filters();
        path = oui::file_dialog(m_application, options);
        if(!path)
        {
            return false;
        }
    }
    return write(*document, *path);
}

bool PaintApp::close(std::size_t index)
{
    if(index >= m_documents.size())
    {
        return true;
    }
    if(m_documents[index]->modified())
    {
        m_tabs->select(index);
        const auto answer{oui::message_box(m_application, "Close",
                                           "Save the changes to " + m_documents[index]->title() + "?",
                                           {"&Save", "&Don't Save", "Cancel"})};
        if(!answer || *answer == 2u || (*answer == 0u && !save(false)))
        {
            return false;
        }
    }
    m_tabs->close_tab(index);
    m_documents.erase(m_documents.begin() + static_cast<std::ptrdiff_t>(index));
    refresh_titles();
    show_layers();
    return true;
}

void PaintApp::use_tool(const ToolEvent& event)
{
    auto* const open{current()};
    if(open == nullptr)
    {
        return;
    }
    auto& document{open->document()};
    const auto& tool{m_tool_panel->selected()};
    if(tool == "color_picker")
    {
        const auto x{static_cast<int>(std::floor(event.at.x))};
        const auto y{static_cast<int>(std::floor(event.at.y))};
        if(x < 0 || y < 0 || x >= document.width() || y >= document.height() || event.phase == ToolEvent::Phase::release)
        {
            return;
        }
        const auto pixel{converted(document.layers()[document.active_layer()].read(x, y, 1, 1), PixelFormat::rgba16)
                             .pixel<Rgba16>(0, 0)};
        const Color color{pixel.value[0] / 65535.0, pixel.value[1] / 65535.0, pixel.value[2] / 65535.0,
                          pixel.value[3] / 65535.0};
        if(event.secondary)
        {
            m_color_panel->set_secondary(color);
        }
        else
        {
            m_color_panel->set_primary(color);
        }
        return;
    }
    if(tool == "move_pixels" || tool == "move_selection")
    {
        move_selected(event, tool == "move_selection");
        return;
    }
    settle();
    if(tool == "rectangle_select" || tool == "ellipse_select")
    {
        select_shape(event, tool == "ellipse_select");
        return;
    }
    const auto painting{tool == "brush" || tool == "pencil" || tool == "eraser"};
    if(!painting || document.layers().empty())
    {
        return;
    }
    switch(event.phase)
    {
    case ToolEvent::Phase::press:
    {
        auto settings{m_tool_options->settings()};
        if(tool == "pencil")
        {
            settings = {1.0, 1.0, false};
        }
        const auto color{event.secondary ? m_color_panel->secondary() : m_color_panel->primary()};
        open->history().begin_step(document);
        m_stroke = std::make_unique<Stroke>(document.layers()[document.active_layer()], document.selection(), color,
                                            settings, tool == "eraser" ? StrokeMode::erase : StrokeMode::paint);
        m_stroke->line_to(event.at);
        break;
    }
    case ToolEvent::Phase::move:
        if(m_stroke != nullptr)
        {
            m_stroke->line_to(event.at);
        }
        break;
    case ToolEvent::Phase::release:
        if(m_stroke != nullptr)
        {
            m_stroke->line_to(event.at);
            open->refresh(m_stroke->take_changed());
            m_stroke.reset();
            const auto title{tool == "eraser" ? "Eraser" : tool == "pencil" ? "Pencil" : "Paintbrush"};
            open->history().finish_step(title, document);
            open->set_modified(true);
            refresh_titles();
        }
        return;
    }
    if(m_stroke != nullptr)
    {
        open->refresh(m_stroke->take_changed());
    }
}

void PaintApp::change_layers(std::string name, const std::function<void(Document&)>& change)
{
    auto* const open{current()};
    if(open == nullptr || m_stroke != nullptr)
    {
        return;
    }
    settle();
    auto& document{open->document()};
    const auto before{LayersState::of(document)};
    open->history().begin_step(document);
    change(document);
    open->history().finish_step(std::move(name), document);
    open->refresh(changed_tiles(before, LayersState::of(document)));
    open->set_modified(true);
    refresh_titles();
    update_ants();
}

void PaintApp::layer_properties()
{
    auto* const open{current()};
    if(open == nullptr)
    {
        return;
    }
    const auto& layer{open->document().layers()[open->document().active_layer()]};
    const auto changed{ask_layer_properties(
        m_application, {layer.name(), layer.visible(), layer.blend_mode(), layer.opacity()})};
    if(!changed)
    {
        return;
    }
    change_layers("Layer Properties",
                  [&changed](Document& document)
                  {
                      auto& target{document.layers()[document.active_layer()]};
                      target.set_name(changed->name);
                      target.set_visible(changed->visible);
                      target.set_blend_mode(changed->blend_mode);
                      target.set_opacity(changed->opacity);
                  });
}

void PaintApp::show_layers()
{
    settle();
    auto* const open{current()};
    m_layers_panel->set_document(open != nullptr ? &open->document() : nullptr);
    update_ants();
}

void PaintApp::select_shape(const ToolEvent& event, bool ellipse)
{
    auto* const open{current()};
    auto& document{open->document()};
    auto& selection{document.selection()};
    if(event.phase == ToolEvent::Phase::press)
    {
        // Paint.NET's modifiers: Ctrl adds, Alt takes away, Alt with the right button intersects, Ctrl with it inverts.
        auto mode{SelectionMode::replace};
        if(event.secondary && event.alt)
        {
            mode = SelectionMode::intersect;
        }
        else if(event.secondary && event.control)
        {
            mode = SelectionMode::invert;
        }
        else if(event.control)
        {
            mode = SelectionMode::add;
        }
        else if(event.alt)
        {
            mode = SelectionMode::subtract;
        }
        open->history().begin_step(document);
        m_selecting = Selecting{event.at, mode, selection.mask()};
        return;
    }
    if(!m_selecting)
    {
        return;
    }
    // From the pixel the press was on to the one the pointer is on, both whole; with Shift, a square.
    auto left{std::floor(std::min(m_selecting->from.x, event.at.x))};
    auto top{std::floor(std::min(m_selecting->from.y, event.at.y))};
    auto right{std::ceil(std::max(m_selecting->from.x, event.at.x))};
    auto bottom{std::ceil(std::max(m_selecting->from.y, event.at.y))};
    if(event.shift)
    {
        const auto side{std::max(right - left, bottom - top)};
        if(event.at.x < m_selecting->from.x)
        {
            left = right - side;
        }
        else
        {
            right = left + side;
        }
        if(event.at.y < m_selecting->from.y)
        {
            top = bottom - side;
        }
        else
        {
            bottom = top + side;
        }
    }
    selection.set_mask(m_selecting->before);
    const auto clicked{right - left <= 1.0 && bottom - top <= 1.0};
    if(!clicked)
    {
        if(ellipse)
        {
            selection.add_ellipse(left, top, right, bottom, m_selecting->mode);
        }
        else
        {
            selection.add_rectangle({static_cast<int>(left), static_cast<int>(top), static_cast<int>(right - left),
                                     static_cast<int>(bottom - top)},
                                    m_selecting->mode);
        }
    }
    if(event.phase == ToolEvent::Phase::release)
    {
        // A click without dragging selects nothing, as in Paint.NET.
        if(clicked && m_selecting->mode == SelectionMode::replace)
        {
            selection.clear();
        }
        m_selecting.reset();
        open->history().finish_step(ellipse ? "Ellipse Select" : "Rectangle Select", document);
    }
    update_ants();
}

void PaintApp::move_selected(const ToolEvent& event, bool outline_only)
{
    auto* const open{current()};
    auto& document{open->document()};
    auto& selection{document.selection()};
    if(event.phase == ToolEvent::Phase::press)
    {
        open->history().begin_step(document);
        auto& layer{document.layers()[document.active_layer()]};
        if(outline_only)
        {
            settle();
            m_moving = Moving{event.at, 0, 0, selection.mask()};
            return;
        }
        if(!m_floating || m_floating->document != open)
        {
            // Lifts what is selected, leaving a hole: the whole layer when nothing is selected.
            Floating floating{open, layer, selection.mask(), lift_selected(layer, selection), 0, 0};
            static_cast<void>(erase_selected(floating.base, selection));
            m_floating = std::move(floating);
        }
        m_moving = Moving{event.at, m_floating->dx, m_floating->dy, {}};
        return;
    }
    if(!m_moving)
    {
        return;
    }
    const auto dx{m_moving->dx + static_cast<int>(std::lround(event.at.x - m_moving->from.x))};
    const auto dy{m_moving->dy + static_cast<int>(std::lround(event.at.y - m_moving->from.y))};
    const auto before{LayersState::of(document)};
    if(outline_only)
    {
        selection.set_mask(m_moving->selection);
        selection.offset(dx, dy);
    }
    else
    {
        auto& layer{document.layers()[document.active_layer()]};
        layer = m_floating->base;
        static_cast<void>(lay_over(layer, m_floating->pixels.pixels, m_floating->pixels.x + dx,
                                   m_floating->pixels.y + dy));
        selection.set_mask(m_floating->selection);
        selection.offset(dx, dy);
        m_floating->dx = dx;
        m_floating->dy = dy;
    }
    open->refresh(changed_tiles(before, LayersState::of(document)));
    if(event.phase == ToolEvent::Phase::release)
    {
        m_moving.reset();
        open->history().finish_step(outline_only ? "Move Selection" : "Move Selected Pixels", document);
        if(!outline_only)
        {
            open->set_modified(true);
            refresh_titles();
        }
    }
    update_ants();
}

void PaintApp::settle()
{
    m_floating.reset();
}

void PaintApp::update_ants()
{
    auto* const open{current()};
    const auto showing{open != nullptr && open->document().selection().active()};
    if(showing && !m_ants.is_active())
    {
        m_ants = m_application.start_timer(std::chrono::milliseconds{150},
                                           [this]
                                           {
                                               m_ants_phase = (m_ants_phase + 1) % 8;
                                               if(auto* const view{current_view()})
                                               {
                                                   view->set_ants_phase(m_ants_phase);
                                               }
                                           });
    }
    else if(!showing && m_ants.is_active())
    {
        m_ants.stop();
    }
}

void PaintApp::undo()
{
    settle();
    if(auto* const open{current()}; open != nullptr && m_stroke == nullptr && open->history().can_undo())
    {
        open->refresh(open->history().undo(open->document()));
        open->set_modified(true);
        refresh_titles();
        update_ants();
    }
}

void PaintApp::redo()
{
    settle();
    if(auto* const open{current()}; open != nullptr && m_stroke == nullptr && open->history().can_redo())
    {
        open->refresh(open->history().redo(open->document()));
        open->set_modified(true);
        refresh_titles();
        update_ants();
    }
}

void PaintApp::edit_color(bool primary)
{
    oui::PromptOptions options;
    options.title = primary ? "Primary Color" : "Secondary Color";
    options.label = "Color, as #rrggbb or #rrggbbaa:";
    options.text = to_hex(primary ? m_color_panel->primary() : m_color_panel->secondary());
    options.validate = [](std::string_view text) -> std::optional<std::string>
    {
        if(!from_hex(text))
        {
            return "Write it as #rrggbb, or #rrggbbaa with its alpha.";
        }
        return std::nullopt;
    };
    if(const auto text{oui::prompt(m_application, options)})
    {
        const auto color{*from_hex(*text)};
        if(primary)
        {
            m_color_panel->set_primary(color);
        }
        else
        {
            m_color_panel->set_secondary(color);
        }
    }
}

void PaintApp::save_all()
{
    const auto selected{m_tabs->selected_index()};
    for(std::size_t index{}; index < m_documents.size(); ++ index)
    {
        if(m_documents[index]->modified() || !m_documents[index]->path())
        {
            m_tabs->select(index);
            if(!save(false))
            {
                break;
            }
        }
    }
    if(selected < m_documents.size())
    {
        m_tabs->select(selected);
    }
}

void PaintApp::quit()
{
    while(!m_documents.empty())
    {
        if(!close(m_documents.size() - 1))
        {
            return;
        }
    }
    m_application.quit();
}

} // namespace oui_paint

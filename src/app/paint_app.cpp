#include "app/paint_app.h"

#include "app/file_filters.h"
#include "app/help.h"
#include "app/fonts.h"
#include "app/recent_files.h"
#include "app/tools.h"
#include "codecs/codecs.h"
#include "codecs/openraster.h"
#include "codecs/palettes.h"
#include "effects/curves.h"
#include "effects/plugin_trial.h"
#include "tools/fill.h"
#include "tools/gradient.h"
#include "tools/shapes.h"
#include "tools/text.h"
#include "ui/canvas_view.h"
#include "ui/color_dialog.h"
#include "ui/color_panel.h"
#include "ui/effect_dialog.h"
#include "ui/help_dialogs.h"
#include "ui/history_panel.h"
#include "ui/layer_properties_dialog.h"
#include "ui/layers_panel.h"
#include "ui/main_frame.h"
#include "ui/new_image_dialog.h"
#include "ui/palettes_panel.h"
#include "ui/plugin_workbench.h"
#include "ui/side_column.h"
#include "ui/side_panel.h"
#include "ui/tool_options.h"
#include "ui/tool_panel.h"
#include "ui/transform_dialogs.h"

#include <oui/dialogs.h>
#include <oui/file_dialog.h>
#include <oui/file_system.h>
#include <oui/layout.h>
#include <oui/utf8.h>
#include <oui/window.h>

#include <algorithm>
#include <cstdlib>
#include <set>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>

using namespace olagarro;

namespace oui_paint
{

namespace
{


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

// Paint.NET's modifiers for selecting: Ctrl adds, Alt takes away, Alt with the right button intersects, Ctrl with it
// inverts.
SelectionMode selection_mode(const ToolEvent& event)
{
    if(event.secondary && event.alt)
    {
        return SelectionMode::intersect;
    }
    if(event.secondary && event.control)
    {
        return SelectionMode::invert;
    }
    if(event.control)
    {
        return SelectionMode::add;
    }
    if(event.alt)
    {
        return SelectionMode::subtract;
    }
    return SelectionMode::replace;
}

// A quarter turn, in radians.
constexpr auto quarter{3.14159265358979323846 / 2.0};

} // namespace

PaintApp::PaintApp(oui::Application& application, oui::PixelBackend& window, oui::Settings& settings)
    : m_application{application}, m_window{window}, m_settings{settings}
{
    std::error_code ignored;
    m_last_folder = std::filesystem::current_path(ignored);
    register_commands();
    build_window();
    load_plugins();
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
    commands.bind("file.quit", {U'q', true});
    commands.bind("file.quit", oui::KeyShortcut{oui::Key::f4, false, true});
    const auto switch_tab{[this](int step)
                          {
                              return [this, step]
                              {
                                  const auto count{static_cast<int>(m_tabs->child_count())};
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
                                      if(static_cast<std::size_t>(number) <= m_tabs->child_count())
                                      {
                                          m_tabs->select(static_cast<std::size_t>(number - 1));
                                      }
                                  });
        commands.bind(id, {static_cast<char32_t>(U'0' + number), false, true});
    }
    add("edit.undo", "&Undo", [this] { if(!edit_text(TextAction::undo)) { undo(); } }, {U'z', true});
    add("edit.redo", "&Redo", [this] { if(!edit_text(TextAction::redo)) { redo(); } }, {U'y', true});
    add("colors.swap", "Swap Colors", [this] { m_color_panel->swap(); }, {U'x'});
    // C, as in Paint.NET: whether a click on a palette sets the primary colour or the secondary one.
    add("colors.toggle_editing", "Edit the Other Color", [this] { m_color_panel->toggle_editing(); }, {U'c'});
    add("edit.cut", "Cu&t",
        [this]
        {
            if(!edit_text(TextAction::cut) && copy(false))
            {
                change_layers("Cut",
                              [](Document& document)
                              {
                                  static_cast<void>(erase_selected(document.layers()[document.active_layer()],
                                                                   document.selection()));
                              });
            }
        },
        {U'x', true});
    add("edit.copy", "&Copy",
        [this]
        {
            if(!edit_text(TextAction::copy))
            {
                static_cast<void>(copy(false));
            }
        },
        {U'c', true});
    add("edit.copy_merged", "Copy M&erged", [this] { static_cast<void>(copy(true)); }, {U'c', true, false, true});
    add("edit.paste", "&Paste",
        [this]
        {
            if(!edit_text(TextAction::paste))
            {
                paste(PasteInto::layer);
            }
        },
        {U'v', true});
    add("edit.paste_new_layer", "Paste into New &Layer", [this] { paste(PasteInto::new_layer); },
        {U'v', true, false, true});
    add("edit.paste_new_image", "Paste into New &Image", [this] { paste(PasteInto::new_image); }, {U'v', true, true});
    commands.register_command("edit.flip_selection_horizontal", "Flip Selection &Horizontal",
                              [this]
                              {
                                  transform_selection("Flip Selection Horizontal",
                                                      [](PieceTransform& t)
                                                      {
                                                          t.radians = -t.radians;
                                                          t.scale_x = -t.scale_x;
                                                      });
                              });
    commands.register_command("edit.flip_selection_vertical", "Flip Selection &Vertical",
                              [this]
                              {
                                  transform_selection("Flip Selection Vertical",
                                                      [](PieceTransform& t)
                                                      {
                                                          t.radians = -t.radians;
                                                          t.scale_y = -t.scale_y;
                                                      });
                              });
    commands.register_command("edit.rotate_selection_clockwise", "Rotate Selection 90° &Clockwise",
                              [this]
                              {
                                  transform_selection("Rotate Selection 90° Clockwise",
                                                      [](PieceTransform& t) { t.radians += quarter; });
                              });
    commands.register_command("edit.rotate_selection_counter_clockwise", "Rotate Selection 90° C&ounter-clockwise",
                              [this]
                              {
                                  transform_selection("Rotate Selection 90° Counter-clockwise",
                                                      [](PieceTransform& t) { t.radians -= quarter; });
                              });
    commands.register_command("edit.rotate_selection_180", "Rotate Selection &180°",
                              [this]
                              {
                                  transform_selection("Rotate Selection 180°",
                                                      [](PieceTransform& t) { t.radians += 2.0 * quarter; });
                              });
    add("edit.select_all", "Select &All",
        [this]
        {
            if(!edit_text(TextAction::select_all))
            {
                change_layers("Select All", [](Document& document) { document.selection().select_all(); });
            }
        },
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
    // F6, as Paint.NET shows and hides its History window; kept for the next time.
    add("view.history_panel", "&History Panel", [this] { show_history_panel(!m_history_panel->is_visible()); },
        oui::Shortcut{0, false, false, false, oui::Key::f6});
    // F7, as Paint.NET shows and hides its Layers window; kept for the next time.
    add("view.layers_panel", "&Layers Panel", [this] { show_layers_panel(!m_frame->right()->is_visible()); },
        oui::Shortcut{0, false, false, false, oui::Key::f7});
    add("help.documentation", "&Documentation", [this] { show_help(m_application, help_pages(m_application.commands())); },
        oui::Shortcut{0, false, false, false, oui::Key::f1});
    commands.register_command("help.about", "&About OUI Paint", [this] { show_about(m_application); });
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
    add("image.crop_to_selection", "Crop to &Selection",
        [this] { change_layers("Crop to Selection", [](Document& document) { document.crop_to_selection(); }); },
        {U'x', true, false, true});
    add("image.resize", "&Resize...",
        [this]
        {
            auto* const open{current()};
            if(open == nullptr)
            {
                return;
            }
            const auto& document{open->document()};
            if(const auto request{ask_resize(m_application, document.width(), document.height())})
            {
                change_layers("Resize Image", [&request](Document& changed)
                              {
                                  changed.resize(request->width, request->height,
                                                 changed.indexed() ? Resampling::nearest : request->resampling);
                              });
            }
        },
        {U'r', true});
    add("image.canvas_size", "Canvas &Size...",
        [this]
        {
            auto* const open{current()};
            if(open == nullptr)
            {
                return;
            }
            const auto& document{open->document()};
            if(const auto request{ask_canvas_size(m_application, document.width(), document.height())})
            {
                change_layers("Canvas Size", [&request](Document& changed)
                              { changed.resize_canvas(request->width, request->height, request->anchor); });
            }
        },
        {U'r', true, false, true});
    commands.register_command("image.flip_horizontal", "Flip &Horizontal",
                              [this] { change_layers("Flip Horizontal", [](Document& d) { d.flip(true); }); });
    commands.register_command("image.flip_vertical", "Flip &Vertical",
                              [this] { change_layers("Flip Vertical", [](Document& d) { d.flip(false); }); });
    add("image.rotate_clockwise", "Rotate 90° &Clockwise",
        [this] { change_layers("Rotate 90° Clockwise", [](Document& d) { d.rotate(1); }); }, {U'h', true});
    add("image.rotate_counter_clockwise", "Rotate 90° C&ounter-clockwise",
        [this] { change_layers("Rotate 90° Counter-clockwise", [](Document& d) { d.rotate(3); }); },
        {U'g', true});
    add("image.rotate_180", "Rotate &180°", [this] { change_layers("Rotate 180°", [](Document& d) { d.rotate(2); }); },
        {U'j', true});
    commands.register_command("layers.flip_horizontal", "Flip Layer &Horizontal",
                              [this, active]
                              {
                                  change_layers("Flip Layer Horizontal",
                                                [&](Document& d) { d.flip_layer(active(), true); });
                              });
    commands.register_command("layers.flip_vertical", "Flip Layer &Vertical",
                              [this, active]
                              {
                                  change_layers("Flip Layer Vertical",
                                                [&](Document& d) { d.flip_layer(active(), false); });
                              });
    add("layers.rotate_zoom", "&Rotate / Zoom...",
        [this, active]
        {
            if(current() == nullptr)
            {
                return;
            }
            if(const auto request{ask_rotate_zoom(m_application)})
            {
                change_layers("Rotate / Zoom",
                              [&](Document& d)
                              {
                                  d.rotate_zoom_layer(active(), request->degrees, request->zoom, request->pan_x,
                                                      request->pan_y,
                                                      d.indexed() ? Resampling::nearest : Resampling::bicubic);
                              });
            }
        },
        {U'z', true, false, true});
    add("image.flatten", "&Flatten",
        [this] { change_layers("Flatten", [](Document& document) { document.flatten_layers(); }); },
        {U'f', true, false, true});
    register_palette_commands();
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
                                          select_tool(*next);
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
    // What is selected as big as it fits, or the whole image when nothing is.
    add("view.zoom_to_selection", "Zoom to &Selection",
        [this]
        {
            if(auto* const canvas{current_view()})
            {
                const auto area{canvas->document().document().selection().bounds()};
                canvas->zoom_to(area.x, area.y, area.width, area.height);
            }
        },
        {U'b', true, false, true});
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
    add_menu("&File", {"file.new", "file.open", oui::menu_submenu("Open &Recent", [this] { return recent_items(); }),
                       oui::menu_separator(), "file.save", "file.save_as", "file.save_all", oui::menu_separator(),
                       "file.close", "file.quit"});
    add_menu("&Edit", {"edit.undo", "edit.redo", oui::menu_separator(), "edit.cut", "edit.copy", "edit.copy_merged",
                       "edit.paste", "edit.paste_new_layer", "edit.paste_new_image", oui::menu_separator(),
                       "edit.erase_selection", "edit.fill_selection", oui::menu_separator(), "edit.select_all",
                       "edit.deselect", "edit.invert_selection", oui::menu_separator(),
                       "edit.flip_selection_horizontal", "edit.flip_selection_vertical",
                       "edit.rotate_selection_clockwise", "edit.rotate_selection_counter_clockwise",
                       "edit.rotate_selection_180"});
    add_menu("&Image", {"image.crop_to_selection", "image.resize", "image.canvas_size", oui::menu_separator(),
                        "image.convert_indexed", "image.convert_color",
                        oui::menu_submenu("&Palettes",
                                          std::vector<oui::MenuItem>{
                                              "palettes.load_setup", "palettes.save_setup", oui::menu_separator(),
                                              "palettes.load_slot", "palettes.save_slot", oui::menu_separator(),
                                              "palettes.add", "palettes.move_up", "palettes.move_down",
                                              oui::menu_separator(), "palettes.load_allowed",
                                              "palettes.clear_allowed"}),
                        oui::menu_separator(), "image.flip_horizontal", "image.flip_vertical", oui::menu_separator(),
                        "image.rotate_clockwise", "image.rotate_counter_clockwise", "image.rotate_180",
                        oui::menu_separator(), "image.flatten"});
    add_menu("&Layers", {"layers.add", "layers.delete", "layers.duplicate", "layers.merge_down", oui::menu_separator(),
                         "layers.move_up", "layers.move_down", "layers.toggle_visible", oui::menu_separator(),
                         "layers.flip_horizontal", "layers.flip_vertical", "layers.rotate_zoom", oui::menu_separator(),
                         "layers.properties"});
    add_menu("&View", {"view.zoom_in", "view.zoom_out", "view.zoom_to_window", "view.zoom_to_selection",
                       "view.actual_size",
                       oui::menu_separator(), "view.history_panel", "view.layers_panel", oui::menu_separator(), "view.interface_bigger",
                       "view.interface_smaller", "view.interface_reset"});
    auto left{oui::column()};
    m_tool_panel = &left->add(std::make_unique<ToolPanel>(tool_buttons(), m_icons));
    m_tool_panel->set_selected("brush");
    m_tool_panel->on_picked([this](const std::string& tool) { select_tool(tool); });
    m_color_panel = &left->add(std::make_unique<ColorPanel>());
    m_color_panel->on_edit([this](bool primary) { edit_color(primary); });
    // A colour picked from the colour panel's palette is used as the nearest index, not one picked before.
    m_color_panel->on_picked(
        [this](bool secondary) { (secondary ? m_secondary_index : m_primary_index).reset(); });
    m_frame->set_left(std::make_unique<SidePanel>(std::move(left)));
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
    auto palettes{std::make_unique<PalettesPanel>()};
    m_palettes_panel = palettes.get();
    m_palettes_panel->on_pick(
        [this](int index, bool right)
        {
            auto* const open{current()};
            if(open == nullptr)
            {
                return;
            }
            // The left button sets the colour being edited, as C chooses it, the right one the other.
            const auto secondary{right != m_color_panel->editing_secondary()};
            const auto& document{open->document()};
            const auto shown{document.palettes()->color(document.layers()[document.active_layer()].palette_slot(),
                                                        index)};
            (secondary ? m_secondary_index : m_primary_index) = index;
            if(secondary)
            {
                m_color_panel->set_secondary(shown);
            }
            else
            {
                m_color_panel->set_primary(shown);
            }
        });
    m_palettes_panel->on_edit(
        [this](int slot, int index)
        {
            auto* const open{current()};
            if(open == nullptr)
            {
                return;
            }
            const auto& setup{*open->document().palettes()};
            const auto& palette{setup.palettes.at(static_cast<std::size_t>(slot))};
            auto start{palette.colors.at(static_cast<std::size_t>(index))};
            start.alpha = 1.0;
            if(const auto chosen{ask_color(m_application, "Palette " + std::to_string(slot + 1) + ", Color " +
                                                              std::to_string(index),
                                           start)})
            {
                change_palettes("Edit Palette Color",
                                [&](PaletteSetup& changed)
                                {
                                    auto color{changed.nearest_allowed(*chosen)};
                                    color.alpha = 1.0;
                                    changed.palettes.at(static_cast<std::size_t>(slot)).colors.at(
                                        static_cast<std::size_t>(index)) = color;
                                });
            }
        });
    m_palettes_panel->on_slot(
        [this](int slot)
        {
            change_layers("Layer Palette",
                          [slot](Document& document) { document.layers()[document.active_layer()].set_palette_slot(slot); });
        });
    auto history{std::make_unique<HistoryPanel>(m_icons)};
    m_history_panel = history.get();
    m_history_panel->on_choose([this](std::size_t position) { history_go_to(position); });
    m_history_panel->on_undo([this] { undo(); });
    m_history_panel->on_redo([this] { redo(); });
    m_frame->set_right(std::make_unique<SidePanel>(
        std::make_unique<SideColumn>(std::move(history), std::move(layers), std::move(palettes))));
    show_layers_panel(m_settings.bool_value("layers_panel", true));
    show_history_panel(m_settings.bool_value("history_panel", true));
    auto options{std::make_unique<ToolOptions>(BrushSettings{}, installed_fonts())};
    m_tool_options = options.get();
    m_frame->set_top(std::move(options));
    auto tabs{std::make_unique<oui::TabView>()};
    m_tabs = tabs.get();
    m_tabs->set_highlight_predicate(
        [this](std::size_t index)
        {
            const auto* const document{document_in_tab(index)};
            return document != nullptr && document->modified();
        });
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
                         (document.indexed() ? "Indexed, " + std::to_string(document.palettes()->count()) + " x " +
                                                   std::to_string(document.palettes()->size) + " colors"
                                             : std::string{describe(document.format())}) +
                         "   " + zoom;
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
    return m_tabs == nullptr ? nullptr : document_in_tab(m_tabs->selected_index());
}

OpenDocument* PaintApp::document_in_tab(std::size_t index)
{
    auto* const view{m_tabs->content_at<CanvasView>(index)};
    return view == nullptr ? nullptr : &view->document();
}

CanvasView* PaintApp::current_view()
{
    return m_tabs == nullptr ? nullptr : m_tabs->selected_content<CanvasView>();
}

void PaintApp::refresh_titles()
{
    for(std::size_t index{}; index < m_tabs->child_count(); ++ index)
    {
        if(const auto* const document{document_in_tab(index)})
        {
            m_tabs->set_title(index, document->modified() ? document->title() + " *" : document->title());
        }
        else if(const auto* const workbench{workbench_in_tab(index)})
        {
            m_tabs->set_title(index, workbench->title());
        }
    }
    refresh_workbench_images();
}

void PaintApp::refresh_workbench_images()
{
    std::vector<std::string> names;
    for(std::size_t index{}; index < m_tabs->child_count(); ++ index)
    {
        if(const auto* const document{document_in_tab(index)})
        {
            names.push_back(document->title());
        }
    }
    // The n-th image open when it is asked for, flattened as it shows.
    const auto image{[this](std::size_t wanted) -> std::optional<Image>
                     {
                         for(std::size_t index{}, seen{}; index < m_tabs->child_count(); ++ index)
                         {
                             const auto* const document{document_in_tab(index)};
                             if(document != nullptr && seen++ == wanted)
                             {
                                 return document->document().flatten();
                             }
                         }
                         return std::nullopt;
                     }};
    for(std::size_t index{}; index < m_tabs->child_count(); ++ index)
    {
        if(auto* const workbench{workbench_in_tab(index)})
        {
            workbench->set_open_images(names, image);
        }
    }
}

void PaintApp::add(std::unique_ptr<OpenDocument> document)
{
    auto view{std::make_unique<CanvasView>(*document)};
    view->on_tool([this](const ToolEvent& event) { take_tool_event(event); });
    view->on_keys([this](const oui::Event& event) { return type_key(event); });
    view->on_pan_key([this] { return m_window.is_key_held(oui::Key::space); });
    view->on_overlay(
        [this](std::uint32_t* pixels, int width, int height)
        {
            const auto moving{m_tool_panel->selected() == "move_pixels"};
            if(!moving && !m_line_edit)
            {
                return;
            }
            // Squares of nine pixels, white with a black edge, where the handles are.
            for(const auto& handle : moving ? floating_handles() : line_handles())
            {
                const auto center_x{static_cast<int>(std::lround(handle.x))};
                const auto center_y{static_cast<int>(std::lround(handle.y))};
                for(int y{-4}; y <= 4; ++ y)
                {
                    for(int x{-4}; x <= 4; ++ x)
                    {
                        const auto px{center_x + x};
                        const auto py{center_y + y};
                        if(px < 0 || py < 0 || px >= width || py >= height)
                        {
                            continue;
                        }
                        const auto edge{std::abs(x) == 4 || std::abs(y) == 4};
                        pixels[static_cast<std::ptrdiff_t>(py) * width + px] = edge ? 0x000000u : 0xffffffu;
                    }
                }
            }
        });
    view->on_pointer(
        [this](bool held)
        {
            const auto& tool{m_tool_panel->selected()};
            if(tool == "pan")
            {
                return held ? oui::Pointer::grabbing : oui::Pointer::hand;
            }
            if(tool == "move_pixels" || tool == "move_selection")
            {
                return oui::Pointer::move;
            }
            if(tool == "text")
            {
                return oui::Pointer::text;
            }
            return oui::Pointer::crosshair;
        });
    m_documents.push_back(std::move(document));
    auto& added{m_tabs->add_tab(m_documents.back()->title(), std::move(view))};
    m_tabs->select(m_tabs->child_count() - 1);
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

void PaintApp::remember(const std::filesystem::path& path)
{
    m_settings.set_list_value("recent_files", with_recent(m_settings.list_value("recent_files"), oui::utf8_of(path)));
    static_cast<void>(m_settings.save());
}

std::vector<oui::MenuItem> PaintApp::recent_items()
{
    std::vector<oui::MenuItem> items;
    const auto recent{m_settings.list_value("recent_files")};
    for(std::size_t index{}; index < recent.size(); ++ index)
    {
        const auto& path{recent[index]};
        // A number to pick it by, then its name and the folder it is in.
        const auto file{oui::path_from_utf8(path)};
        const auto* const home{std::getenv("HOME") != nullptr ? std::getenv("HOME") : std::getenv("USERPROFILE")};
        const auto title{"&" + std::to_string((index + 1) % 10) + "  " + oui::utf8_of(file.filename()) + "   " +
                         short_folder(oui::utf8_of(file.parent_path()),
                                      home != nullptr ? oui::utf8_of(oui::path_from_utf8(home)) : std::string{})};
        items.push_back(oui::menu_action(title,
                                         [this, path]
                                         {
                                             const auto file{oui::path_from_utf8(path)};
                                             std::error_code ignored;
                                             if(!std::filesystem::exists(file, ignored))
                                             {
                                                 show_error("Open Recent", path + " is not there any more.");
                                                 auto kept{m_settings.list_value("recent_files")};
                                                 kept.erase(std::remove(kept.begin(), kept.end(), path), kept.end());
                                                 m_settings.set_list_value("recent_files", kept);
                                                 static_cast<void>(m_settings.save());
                                                 return;
                                             }
                                             static_cast<void>(open(file));
                                         }));
    }
    if(items.empty())
    {
        items.push_back(oui::menu_action("(none yet)", [] {}, false));
    }
    return items;
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
    if(file_format_of(oui::utf8_of(path)) == FileFormat::openraster)
    {
        auto layered{read_openraster(*bytes, error)};
        if(!layered)
        {
            show_error("Open", oui::utf8_of(path.filename()) + ": " + error);
            return false;
        }
        auto document{std::make_unique<OpenDocument>(std::move(*layered))};
        document->set_path(path);
        remember(path);
        m_last_folder = path.parent_path();
        add(std::move(document));
        return true;
    }
    std::optional<Image> image;
    if(file_format_of(oui::utf8_of(path)) == FileFormat::raw16)
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
        if(file_format_of(oui::utf8_of(path)) == FileFormat::png && is_paletted_png(*bytes))
        {
            // A PNG with a palette opens indexed, its indices as they are.
            auto indexed{read_indexed_png(*bytes, error)};
            if(!indexed)
            {
                show_error("Open", oui::utf8_of(path.filename()) + ": " + error);
                return false;
            }
            auto document{std::make_unique<OpenDocument>(std::move(*indexed))};
            document->set_path(path);
            remember(path);
            m_last_folder = path.parent_path();
            add(std::move(document));
            return true;
        }
        image = read_image(*bytes, error);
    }
    if(!image)
    {
        show_error("Open", oui::utf8_of(path.filename()) + ": " + error);
        return false;
    }
    auto document{std::make_unique<OpenDocument>(Document::from_image(*image))};
    document->set_path(path);
    remember(path);
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
    const auto format{file_format_of(oui::utf8_of(path))};
    if(!format || !can_write(*format))
    {
        show_error("Save", "OUI Paint cannot write " + oui::utf8_of(path.filename()) +
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
        remember(path);
        tell_saved(path);
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
    if(document.document().indexed())
    {
        auto as_indices{false};
        if(*format == FileFormat::png)
        {
            const auto answer{oui::message_box(m_application, "Save",
                                               "Save as an indexed PNG, with every palette in order, or in colours?",
                                               {"&Indexed", "In &Colors", "Cancel"})};
            if(!answer || *answer == 2u)
            {
                return false;
            }
            as_indices = *answer == 0u;
        }
        std::string error;
        try
        {
            const auto bytes{as_indices ? write_indexed_png(document.document())
                                        : write_image(converted(document.document().flatten(),
                                                                written_format(*format, PixelFormat::rgba8)),
                                                      *format)};
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
        remember(path);
        tell_saved(path);
        return true;
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
    remember(path);
    tell_saved(path);
    return true;
}

void PaintApp::tell_saved(const std::filesystem::path& path)
{
    try
    {
        m_plugins->saved(oui::utf8_of(path));
    }
    catch(const std::exception& failure)
    {
        show_error("After Saving", failure.what());
    }
}

bool PaintApp::save(bool ask_for_path)
{
    if(auto* const workbench{workbench_in_tab(m_tabs->selected_index())})
    {
        return save_workbench(*workbench, ask_for_path);
    }
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
        options.file_name = path ? oui::utf8_of(path->filename())
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
    if(auto* const workbench{workbench_in_tab(index)})
    {
        if(workbench->modified())
        {
            m_tabs->select(index);
            const auto answer{oui::message_box(m_application, "Close", "Save the plugin " + workbench->title() + "?",
                                               {"&Save", "&Don't Save", "Cancel"})};
            if(!answer || *answer == 2u || (*answer == 0u && !save_workbench(*workbench, false)))
            {
                return false;
            }
        }
        m_tabs->close_tab(index);
        refresh_titles();
        show_layers();
        return true;
    }
    auto* const document{document_in_tab(index)};
    if(document == nullptr)
    {
        return true;
    }
    if(m_line_edit && m_line_edit->document == document)
    {
        finish_line();
    }
    if(document->modified())
    {
        m_tabs->select(index);
        const auto answer{oui::message_box(m_application, "Close", "Save the changes to " + document->title() + "?",
                                           {"&Save", "&Don't Save", "Cancel"})};
        if(!answer || *answer == 2u || (*answer == 0u && !save(false)))
        {
            return false;
        }
    }
    m_tabs->close_tab(index);
    m_documents.erase(std::find_if(m_documents.begin(), m_documents.end(),
                                   [document](const auto& open) { return open.get() == document; }));
    refresh_titles();
    show_layers();
    return true;
}

void PaintApp::take_tool_event(const ToolEvent& event)
{
    // Tools that draw a box or a line from where the drag began only need where the pointer is now: of the moves a
    // frame brings, the last one is used, so a slow redraw does not fall behind the pointer. Tools that trace the
    // pointer's path get every move.
    const auto& tool{m_tool_panel->selected()};
    const auto traces{tool == "brush" || tool == "pencil" || tool == "eraser" || tool == "clone_stamp" ||
                      tool == "recolor" || tool == "lasso_select"};
    if(event.phase == ToolEvent::Phase::move && !traces)
    {
        if(!m_pending_move)
        {
            m_application.post([this] { use_pending_move(); });
        }
        m_pending_move = event;
        return;
    }
    use_pending_move();
    use_tool(event);
}

void PaintApp::use_pending_move()
{
    if(const auto pending{std::exchange(m_pending_move, std::nullopt)})
    {
        use_tool(*pending);
    }
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
        const auto& picked_layer{document.layers()[document.active_layer()]};
        const auto read{picked_layer.read(x, y, 1, 1)};
        if(document.indexed())
        {
            // The index itself, the see-through one where the pixel is see-through.
            const auto& palette{document.palettes()->palettes.at(static_cast<std::size_t>(picked_layer.palette_slot()))};
            const auto value{read.pixel<GrayAlpha8>(0, 0)};
            const auto index{value.alpha() == 0 ? palette.transparent.value_or(0) : static_cast<int>(value.value[0])};
            (event.secondary ? m_secondary_index : m_primary_index) = index;
            const auto shown{document.palettes()->color(picked_layer.palette_slot(), index)};
            if(event.secondary)
            {
                m_color_panel->set_secondary(shown);
            }
            else
            {
                m_color_panel->set_primary(shown);
            }
            return;
        }
        const auto pixel{converted(read, PixelFormat::rgba16).pixel<Rgba16>(0, 0)};
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
    if(tool == "pan")
    {
        if(event.phase != ToolEvent::Phase::press && m_panning)
        {
            open->viewport().scroll_by(event.in_view.x - m_panning->x, event.in_view.y - m_panning->y);
        }
        m_panning = event.phase == ToolEvent::Phase::release ? std::nullopt : std::optional<Position>{event.in_view};
        return;
    }
    if(tool == "zoom")
    {
        // A click zooms in where it is, a right click out.
        if(event.phase == ToolEvent::Phase::press)
        {
            if(event.secondary)
            {
                open->viewport().zoom_out(event.in_view);
            }
            else
            {
                open->viewport().zoom_in(event.in_view);
            }
        }
        return;
    }
    if(tool == "move_pixels" || tool == "move_selection")
    {
        move_selected(event, tool == "move_selection");
        return;
    }
    // The Text tool keeps typing across its own clicks, Line / Curve the line it bends; any other tool puts down what
    // was floating.
    if(tool != "text")
    {
        settle(tool == "line");
    }
    if(tool == "rectangle_select" || tool == "ellipse_select")
    {
        select_shape(event, tool == "ellipse_select");
        return;
    }
    if(tool == "lasso_select")
    {
        auto& selection{document.selection()};
        if(event.phase == ToolEvent::Phase::press)
        {
            open->history().begin_step(document);
            m_selecting = Selecting{event.at, selection_mode(event), selection.mask()};
            m_lasso = {event.at};
            return;
        }
        if(!m_selecting)
        {
            return;
        }
        m_lasso.push_back(event.at);
        selection.set_mask(m_selecting->before);
        selection.add_polygon(m_lasso, m_selecting->mode);
        if(event.phase == ToolEvent::Phase::release)
        {
            m_selecting.reset();
            m_lasso.clear();
            open->history().finish_step("Lasso Select", document);
        }
        update_ants();
        return;
    }
    if(tool == "magic_wand")
    {
        const auto x{static_cast<int>(std::floor(event.at.x))};
        const auto y{static_cast<int>(std::floor(event.at.y))};
        if(event.phase != ToolEvent::Phase::press || x < 0 || y < 0 || x >= document.width() ||
           y >= document.height())
        {
            return;
        }
        open->history().begin_step(document);
        document.selection().add_coverage(fill_region(document, x, y, m_tool_options->fill()), selection_mode(event));
        open->history().finish_step("Magic Wand", document);
        update_ants();
        return;
    }
    if(tool == "bucket")
    {
        const auto x{static_cast<int>(std::floor(event.at.x))};
        const auto y{static_cast<int>(std::floor(event.at.y))};
        if(event.phase != ToolEvent::Phase::press || x < 0 || y < 0 || x >= document.width() ||
           y >= document.height())
        {
            return;
        }
        const auto before{LayersState::of(document)};
        open->history().begin_step(document);
        auto fill{m_tool_options->fill()};
        fill.exact = document.indexed();
        static_cast<void>(bucket_fill(document, x, y, ink(event.secondary), fill));
        open->history().finish_step("Paint Bucket", document);
        open->refresh(changed_tiles(before, LayersState::of(document)));
        open->set_modified(true);
        refresh_titles();
        return;
    }
    if(tool == "gradient" || tool == "line" || tool == "shapes")
    {
        drag_draw(event, tool);
        return;
    }
    if(tool == "text" && event.phase == ToolEvent::Phase::press)
    {
        // A click puts down what was typed and starts typing anew where it is.
        settle();
        open->history().begin_step(document);
        m_typing = Typing{open, {std::floor(event.at.x), std::floor(event.at.y)}, {},
                          document.layers()[document.active_layer()]};
        if(auto* const view{current_view()})
        {
            view->claim_keys(true);
        }
        show_typing(true);
        return;
    }
    if(tool == "clone_stamp" && event.phase == ToolEvent::Phase::press && event.control)
    {
        m_clone_source = event.at;
        m_clone_offset.reset();
        return;
    }
    const auto painting{tool == "brush" || tool == "pencil" || tool == "eraser" || tool == "clone_stamp" ||
                        tool == "recolor"};
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
        // Indices are painted whole: no antialiasing and no mixing.
        settings.exact = document.indexed();
        auto see_through{false};
        const auto color{ink(event.secondary, &see_through)};
        open->history().begin_step(document);
        auto mode{tool == "eraser" || see_through ? StrokeMode::erase : StrokeMode::paint};
        StrokeSource source;
        if(tool == "clone_stamp")
        {
            if(!m_clone_source)
            {
                open->history().cancel_step();
                show_error("Clone Stamp", "Hold Ctrl and click where to copy from first.");
                return;
            }
            if(!m_clone_offset)
            {
                m_clone_offset = std::make_pair(static_cast<int>(std::lround(m_clone_source->x - event.at.x)),
                                                static_cast<int>(std::lround(m_clone_source->y - event.at.y)));
            }
            mode = StrokeMode::clone;
            source.dx = m_clone_offset->first;
            source.dy = m_clone_offset->second;
        }
        else if(tool == "recolor")
        {
            // The secondary colour becomes the primary, or the other way round with the right button.
            mode = StrokeMode::recolor;
            source.match = ink(!event.secondary);
            source.tolerance = m_tool_options->fill().tolerance;
        }
        m_stroke = std::make_unique<Stroke>(document.layers()[document.active_layer()], document.selection(), color,
                                            settings, mode, source);
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
            const auto title{tool == "eraser"        ? "Eraser"
                             : tool == "pencil"      ? "Pencil"
                             : tool == "clone_stamp" ? "Clone Stamp"
                             : tool == "recolor"     ? "Recolor"
                                                     : "Paintbrush"};
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
    m_history_panel->set_history(open != nullptr ? &open->history() : nullptr);
    m_palettes_panel->set_document(open != nullptr ? &open->document() : nullptr);
    update_ants();
}

void PaintApp::select_tool(const std::string& tool)
{
    settle();
    m_tool_panel->set_selected(tool);
    m_tool_options->show_for(tool);
}

void PaintApp::drag_draw(const ToolEvent& event, const std::string& tool)
{
    auto* const open{current()};
    auto& document{open->document()};
    auto& layer{document.layers()[document.active_layer()]};
    // A line still to bend: a press on one of its points takes it, and dragging it bends the line; a press elsewhere
    // leaves the line as it is and starts another.
    if(tool == "line" && m_line_edit && m_line_edit->document == open)
    {
        if(event.phase == ToolEvent::Phase::press)
        {
            const auto handles{line_handles()};
            for(std::size_t index{}; index < handles.size(); ++ index)
            {
                if(std::abs(handles[index].x - event.in_view.x) <= 6.0 &&
                   std::abs(handles[index].y - event.in_view.y) <= 6.0)
                {
                    m_line_edit->held = index;
                    return;
                }
            }
            finish_line();
        }
        else if(m_line_edit->held)
        {
            m_line_edit->points[*m_line_edit->held] = event.at;
            redraw_line();
            if(event.phase == ToolEvent::Phase::release)
            {
                m_line_edit->held.reset();
            }
            return;
        }
    }
    if(event.phase == ToolEvent::Phase::press)
    {
        open->history().begin_step(document);
        m_dragging = Dragging{event.at, layer, event.secondary};
    }
    if(!m_dragging)
    {
        return;
    }
    // The right button swaps the colours, as in Paint.NET.
    auto first_see_through{false};
    auto second_see_through{false};
    const auto first{ink(m_dragging->secondary, &first_see_through)};
    const auto second{ink(!m_dragging->secondary, &second_see_through)};
    auto brush{m_tool_options->settings()};
    brush.antialias = brush.antialias && !document.indexed();
    auto to{event.at};
    const auto from{m_dragging->from};
    if(event.shift && tool == "line")
    {
        // In steps of 15 degrees.
        constexpr auto step{3.14159265358979323846 / 12.0};
        const auto angle{std::round(std::atan2(to.y - from.y, to.x - from.x) / step) * step};
        const auto length{std::hypot(to.x - from.x, to.y - from.y)};
        to = {from.x + std::cos(angle) * length, from.y + std::sin(angle) * length};
    }
    else if(event.shift && tool == "shapes")
    {
        // A square or a circle.
        const auto side{std::max(std::abs(to.x - from.x), std::abs(to.y - from.y))};
        to = {from.x + (to.x < from.x ? -side : side), from.y + (to.y < from.y ? -side : side)};
    }
    const auto before{LayersState::of(document)};
    layer = m_dragging->base;
    if(tool == "gradient" && document.indexed())
    {
        // Between the two indices, dithered.
        const auto pixel{[](const Color& color, bool see_through)
                         {
                             return GrayAlpha8{{static_cast<std::uint8_t>(std::lround(color.red * 255.0)),
                                                static_cast<std::uint8_t>(see_through ? 0 : 255)}};
                         }};
        static_cast<void>(draw_dithered_gradient(layer, m_dragging->base, document.selection(),
                                                 m_tool_options->gradient(), from, to, pixel(first, first_see_through),
                                                 pixel(second, second_see_through)));
    }
    else if(tool == "gradient")
    {
        static_cast<void>(draw_gradient(layer, m_dragging->base, document.selection(), m_tool_options->gradient(), from,
                                        to, first, second, m_tool_options->gradient_transparency()));
    }
    else if(tool == "line")
    {
        static_cast<void>(draw_line(layer, document.selection(), from, to, first, brush.width, brush.antialias,
                                    m_tool_options->line()));
    }
    else
    {
        const auto shape{m_tool_options->shape()};
        static_cast<void>(draw_shape(layer, document.selection(), shape.kind, shape.style, from, to, first, second,
                                     brush.width, brush.antialias));
    }
    open->refresh(changed_tiles(before, LayersState::of(document)));
    if(event.phase == ToolEvent::Phase::release && tool == "line")
    {
        // Drawn, it waits to be bent: its handles a third and two thirds along it.
        const auto third{[&](double part)
                         { return Position{from.x + (to.x - from.x) * part, from.y + (to.y - from.y) * part}; }};
        m_line_edit = LineEdit{open,
                               m_dragging->base,
                               {from, third(1.0 / 3.0), third(2.0 / 3.0), to},
                               first,
                               brush.width,
                               brush.antialias,
                               m_tool_options->line(),
                               std::nullopt};
        m_dragging.reset();
        return;
    }
    if(event.phase == ToolEvent::Phase::release)
    {
        m_dragging.reset();
        open->history().finish_step(tool == "gradient" ? "Gradient" : "Shapes", document);
        open->set_modified(true);
        refresh_titles();
    }
}

void PaintApp::redraw_line()
{
    auto& edit{*m_line_edit};
    auto& document{edit.document->document()};
    auto& layer{document.layers()[document.active_layer()]};
    const auto before{LayersState::of(document)};
    layer = edit.base;
    static_cast<void>(
        draw_curve(layer, document.selection(), edit.points, edit.color, edit.width, edit.antialias, edit.settings));
    edit.document->refresh(changed_tiles(before, LayersState::of(document)));
}

void PaintApp::finish_line()
{
    if(!m_line_edit)
    {
        return;
    }
    auto& open{*m_line_edit->document};
    m_line_edit.reset();
    open.history().finish_step("Line / Curve", open.document());
    open.set_modified(true);
    refresh_titles();
}

std::vector<Position> PaintApp::line_handles()
{
    std::vector<Position> handles;
    if(!m_line_edit || m_line_edit->document != current())
    {
        return handles;
    }
    const auto& viewport{m_line_edit->document->viewport()};
    for(const auto& point : m_line_edit->points)
    {
        handles.push_back({viewport.origin().x + point.x * viewport.zoom(), viewport.origin().y + point.y * viewport.zoom()});
    }
    return handles;
}

void PaintApp::select_shape(const ToolEvent& event, bool ellipse)
{
    auto* const open{current()};
    auto& document{open->document()};
    auto& selection{document.selection()};
    if(event.phase == ToolEvent::Phase::press)
    {
        const auto mode{selection_mode(event)};
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

void PaintApp::ensure_floating()
{
    auto* const open{current()};
    if(open == nullptr || (m_floating && m_floating->document == open))
    {
        return;
    }
    auto& document{open->document()};
    auto& layer{document.layers()[document.active_layer()]};
    // Lifts what is selected, leaving a hole: the whole layer when nothing is selected.
    Floating floating{open, layer, lift_piece(layer, document.selection()), {}};
    static_cast<void>(erase_selected(floating.base, document.selection()));
    m_floating = std::move(floating);
}

void PaintApp::place_floating()
{
    auto& open{*m_floating->document};
    auto& document{open.document()};
    const auto before{LayersState::of(document)};
    place_piece(document.layers()[document.active_layer()], m_floating->base, document.selection(),
                m_floating->piece, m_floating->transform, document.indexed());
    open.refresh(changed_tiles(before, LayersState::of(document)));
    update_ants();
}

void PaintApp::transform_selection(std::string name, const std::function<void(PieceTransform&)>& change)
{
    auto* const open{current()};
    if(open == nullptr || m_stroke != nullptr)
    {
        return;
    }
    auto& document{open->document()};
    open->history().begin_step(document);
    ensure_floating();
    change(m_floating->transform);
    place_floating();
    open->history().finish_step(std::move(name), document);
    open->set_modified(true);
    refresh_titles();
}

std::vector<Position> PaintApp::floating_handles()
{
    std::vector<Position> handles;
    auto* const open{current()};
    if(!m_floating || m_floating->document != open)
    {
        return handles;
    }
    const auto corners{piece_corners(m_floating->piece, m_floating->transform)};
    for(const auto& corner : corners)
    {
        handles.push_back(open->viewport().view_at(corner));
    }
    for(std::size_t index{}; index < 4; ++ index)
    {
        const auto& a{corners[index]};
        const auto& b{corners[(index + 1) % 4]};
        handles.push_back(open->viewport().view_at({(a.x + b.x) / 2.0, (a.y + b.y) / 2.0}));
    }
    return handles;
}

void PaintApp::move_selected(const ToolEvent& event, bool outline_only)
{
    auto* const open{current()};
    auto& document{open->document()};
    auto& selection{document.selection()};
    if(event.phase == ToolEvent::Phase::press)
    {
        open->history().begin_step(document);
        if(outline_only)
        {
            settle();
            m_moving = Moving{Moving::Kind::outline, event.at, {}, 0, selection.mask()};
            return;
        }
        // A handle pressed scales; the right button turns; anything else moves.
        auto kind{event.secondary ? Moving::Kind::rotate : Moving::Kind::move};
        auto handle{0};
        const auto handles{floating_handles()};
        for(std::size_t index{}; index < handles.size() && !event.secondary; ++ index)
        {
            if(std::abs(handles[index].x - event.in_view.x) <= 6.0 && std::abs(handles[index].y - event.in_view.y) <= 6.0)
            {
                kind = Moving::Kind::scale;
                handle = static_cast<int>(index);
                break;
            }
        }
        ensure_floating();
        m_moving = Moving{kind, event.at, m_floating->transform, handle, {}};
        return;
    }
    if(!m_moving)
    {
        return;
    }
    const auto& start{m_moving->start};
    if(m_moving->kind == Moving::Kind::outline)
    {
        const auto before{LayersState::of(document)};
        selection.set_mask(m_moving->selection);
        selection.offset(static_cast<int>(std::lround(event.at.x - m_moving->from.x)),
                         static_cast<int>(std::lround(event.at.y - m_moving->from.y)));
        open->refresh(changed_tiles(before, LayersState::of(document)));
        update_ants();
    }
    else
    {
        auto& transform{m_floating->transform};
        const auto& piece{m_floating->piece};
        const auto center_x{piece.x + piece.pixels.width / 2.0 + start.dx};
        const auto center_y{piece.y + piece.pixels.height / 2.0 + start.dy};
        switch(m_moving->kind)
        {
        case Moving::Kind::move:
            // By whole pixels, so a piece moved and nothing else keeps its pixels exactly.
            transform.dx = start.dx + std::round(event.at.x - m_moving->from.x);
            transform.dy = start.dy + std::round(event.at.y - m_moving->from.y);
            break;
        case Moving::Kind::rotate:
        {
            const auto turned{std::atan2(event.at.y - center_y, event.at.x - center_x) -
                              std::atan2(m_moving->from.y - center_y, m_moving->from.x - center_x)};
            transform.radians = start.radians + turned;
            if(event.shift)
            {
                constexpr auto step{3.14159265358979323846 / 12.0};
                transform.radians = std::round(transform.radians / step) * step;
            }
            break;
        }
        case Moving::Kind::scale:
        {
            // In the piece's own frame, unturned: the handle follows the pointer and the one across stays.
            const auto cosine{std::cos(start.radians)};
            const auto sine{std::sin(start.radians)};
            const auto px{event.at.x - center_x};
            const auto py{event.at.y - center_y};
            const auto local_x{px * cosine + py * sine};
            const auto local_y{-px * sine + py * cosine};
            const auto half_width{piece.pixels.width / 2.0};
            const auto half_height{piece.pixels.height / 2.0};
            // Which way the handle is from the middle, -1, 0 or 1 on each axis.
            constexpr int across[8]{-1, 1, 1, -1, 0, 1, 0, -1};
            constexpr int down[8]{-1, -1, 1, 1, -1, 0, 1, 0};
            const auto hx{across[m_moving->handle]};
            const auto hy{down[m_moving->handle]};
            auto scale_x{start.scale_x};
            auto scale_y{start.scale_y};
            auto middle_x{0.0};
            auto middle_y{0.0};
            if(hx != 0)
            {
                const auto fixed{-hx * half_width * start.scale_x};
                scale_x = (local_x - fixed) / (2.0 * hx * half_width);
                middle_x = (local_x + fixed) / 2.0;
            }
            if(hy != 0)
            {
                const auto fixed{-hy * half_height * start.scale_y};
                scale_y = (local_y - fixed) / (2.0 * hy * half_height);
                middle_y = (local_y + fixed) / 2.0;
            }
            if(event.shift && hx != 0 && hy != 0 && start.scale_x != 0.0)
            {
                // Its proportions kept, by the larger change.
                const auto ratio{start.scale_y / start.scale_x};
                if(std::abs(scale_x / start.scale_x) > std::abs(scale_y / start.scale_y))
                {
                    scale_y = scale_x * ratio;
                }
                else
                {
                    scale_x = scale_y / ratio;
                }
                middle_x = (hx * half_width * scale_x - hx * half_width * start.scale_x) / 2.0;
                middle_y = (hy * half_height * scale_y - hy * half_height * start.scale_y) / 2.0;
            }
            transform.scale_x = std::abs(scale_x) < 1e-3 ? 1e-3 : scale_x;
            transform.scale_y = std::abs(scale_y) < 1e-3 ? 1e-3 : scale_y;
            // The middle moves with the handle, turned back into the image's frame.
            transform.dx = start.dx + middle_x * cosine - middle_y * sine;
            transform.dy = start.dy + middle_x * sine + middle_y * cosine;
            break;
        }
        case Moving::Kind::outline:
            break;
        }
        place_floating();
    }
    if(event.phase == ToolEvent::Phase::release)
    {
        const auto outline{m_moving->kind == Moving::Kind::outline};
        m_moving.reset();
        open->history().finish_step(outline ? "Move Selection" : "Move Selected Pixels", document);
        if(!outline)
        {
            open->set_modified(true);
            refresh_titles();
        }
    }
}

bool PaintApp::copy(bool merged)
{
    auto* const open{current()};
    if(open == nullptr)
    {
        return false;
    }
    settle();
    const auto& document{open->document()};
    const auto& selection{document.selection()};
    if(selection.bounds().empty())
    {
        return false;
    }
    const auto& active{document.layers()[document.active_layer()]};
    const auto flat{merged ? document.flatten() : Image{}};
    Layer source{merged ? flat.format : document.format(), "Copy"};
    if(merged)
    {
        source.write(flat, 0, 0);
    }
    const auto lifted{lift_selected(merged ? source : active, selection)};
    // Other programs take colours, never indices.
    const auto pixels{merged ? lifted.pixels : document.colors_of(lifted.pixels, active.palette_slot())};
    // As PNG, which keeps transparency, and as BMP over white for programs that take nothing else.
    const auto png{write_image(pixels, FileFormat::png)};
    const auto bmp{write_image(converted(pixels, written_format(FileFormat::bmp, pixels.format)), FileFormat::bmp)};
    if(!m_application.clipboard().set_data({{"image/png", png}, {"image/bmp", bmp}}))
    {
        show_error("Copy", "The clipboard did not take the image.");
        return false;
    }
    return true;
}

std::optional<Image> PaintApp::clipboard_image()
{
    auto& clipboard{m_application.clipboard()};
    const auto types{clipboard.types()};
    for(const auto* const type : {"image/png", "image/bmp", "image/x-bmp", "image/jpeg", "image/gif", "image/x-tga"})
    {
        if(std::find(types.begin(), types.end(), type) == types.end())
        {
            continue;
        }
        if(const auto bytes{clipboard.data(type)})
        {
            std::string error;
            if(auto image{read_image(*bytes, error)})
            {
                return image;
            }
        }
    }
    return std::nullopt;
}

void PaintApp::paste(PasteInto into)
{
    settle();
    auto image{clipboard_image()};
    if(!image)
    {
        show_error("Paste", "The clipboard holds no image.");
        return;
    }
    if(into == PasteInto::new_image || current() == nullptr)
    {
        auto document{std::make_unique<OpenDocument>(Document::from_image(*image))};
        document->set_untitled_number(++ m_untitled_count);
        add(std::move(document));
        return;
    }
    auto* const open{current()};
    auto& document{open->document()};
    const auto pasted{document.indices_of(*image, document.layers()[document.active_layer()].palette_slot())};
    auto grow{false};
    if(pasted.width > document.width() || pasted.height > document.height())
    {
        const auto answer{oui::message_box(m_application, "Paste",
                                           "The image being pasted is bigger than the canvas.\n"
                                           "Expand the canvas to fit it?",
                                           {"&Expand Canvas", "&Keep Canvas Size", "Cancel"})};
        if(!answer || *answer == 2u)
        {
            return;
        }
        grow = *answer == 0u;
    }
    // At the top left of what shows, as Paint.NET pastes, or of the image when the canvas grew.
    auto x{0};
    auto y{0};
    if(!grow)
    {
        const auto corner{open->viewport().image_at({0.0, 0.0})};
        x = std::clamp(static_cast<int>(std::floor(corner.x)), 0, std::max(0, document.width() - pasted.width));
        y = std::clamp(static_cast<int>(std::floor(corner.y)), 0, std::max(0, document.height() - pasted.height));
    }
    const auto before{LayersState::of(document)};
    open->history().begin_step(document);
    if(grow)
    {
        document.set_size(std::max(document.width(), pasted.width), std::max(document.height(), pasted.height));
    }
    if(into == PasteInto::new_layer)
    {
        document.insert_layer_above("Layer " + std::to_string(document.layers().size() + 1));
    }
    auto& layer{document.layers()[document.active_layer()]};
    // A piece all of it selected, floating over the layer as it was.
    Image mask{PixelFormat::gray_alpha8, pasted.width, pasted.height};
    for(int row{}; row < pasted.height; ++ row)
    {
        for(int column{}; column < pasted.width; ++ column)
        {
            mask.set_pixel(column, row, GrayAlpha8{{255, 255}});
        }
    }
    m_floating = Floating{open, layer, FloatingPiece{pasted, mask, x, y}, {}};
    place_floating();
    open->history().finish_step(into == PasteInto::new_layer ? "Paste into New Layer" : "Paste", document);
    // What is pasted can be moved straight away.
    m_tool_panel->set_selected("move_pixels");
    m_tool_options->show_for("move_pixels");
    open->refresh(changed_tiles(before, LayersState::of(document)));
    open->set_modified(true);
    refresh_titles();
    update_ants();
}

oui::Font* PaintApp::text_font()
{
    const auto* const family{m_tool_options->font()};
    if(family == nullptr)
    {
        return nullptr;
    }
    const auto size{static_cast<int>(std::lround(m_tool_options->font_size()))};
    const auto key{std::make_pair(family->path, size)};
    if(const auto found{m_fonts.find(key)}; found != m_fonts.end())
    {
        return found->second.get();
    }
    auto& bytes{m_font_files[family->path]};
    if(bytes.empty())
    {
        std::string error;
        if(const auto read{read_file(family->path, error)})
        {
            bytes = *read;
        }
    }
    try
    {
        return m_fonts.emplace(key, oui::truetype_font(bytes, static_cast<float>(size))).first->second.get();
    }
    catch(const std::exception&)
    {
        return nullptr;
    }
}

void PaintApp::show_typing(bool with_cursor)
{
    if(!m_typing)
    {
        return;
    }
    auto& open{*m_typing->document};
    auto& document{open.document()};
    auto& layer{document.layers()[document.active_layer()]};
    const auto before{LayersState::of(document)};
    layer = m_typing->base;
    if(auto* const font{text_font()})
    {
        const auto color{ink(false)};
        static_cast<void>(draw_text(layer, document.selection(), *font, m_typing->text, m_typing->at, color,
                                    m_tool_options->settings().antialias && !document.indexed()));
        if(with_cursor)
        {
            // A thin line after the last character, as tall as a line of text.
            const auto last_line{m_typing->text.substr(m_typing->text.rfind('\n') == std::string::npos
                                                           ? 0
                                                           : m_typing->text.rfind('\n') + 1)};
            const auto lines{std::count(m_typing->text.begin(), m_typing->text.end(), '\n')};
            const auto x{m_typing->at.x + font->measure(last_line) + 1.0};
            const auto top{m_typing->at.y + static_cast<double>(lines) * font->line_height()};
            static_cast<void>(draw_line(layer, document.selection(), {x, top}, {x, top + font->line_height()}, color,
                                        1.0, false, {}));
        }
    }
    open.refresh(changed_tiles(before, LayersState::of(document)));
}

bool PaintApp::type_key(const oui::Event& event)
{
    // Enter leaves a line being bent as it is.
    if(const auto* const key{std::get_if<oui::KeyEvent>(&event)};
       m_line_edit && key != nullptr && (key->key == oui::Key::enter || key->key == oui::Key::escape))
    {
        finish_line();
        return true;
    }
    if(!m_typing)
    {
        return false;
    }
    auto& text{m_typing->text};
    if(const auto* const typed{std::get_if<oui::TextEvent>(&event)})
    {
        if(typed->control || typed->alt)
        {
            return false;
        }
        text += oui::encode_utf8(typed->codepoint);
    }
    else if(const auto* const key{std::get_if<oui::KeyEvent>(&event)})
    {
        switch(key->key)
        {
        case oui::Key::enter:
            text += '\n';
            break;
        case oui::Key::space:
            text += ' ';
            break;
        case oui::Key::backspace:
            // The last character, whatever bytes it takes.
            while(!text.empty() && (static_cast<unsigned char>(text.back()) & 0xc0u) == 0x80u)
            {
                text.pop_back();
            }
            if(!text.empty())
            {
                text.pop_back();
            }
            break;
        case oui::Key::escape:
            settle();
            return true;
        default:
            return false;
        }
    }
    else
    {
        return false;
    }
    show_typing(true);
    return true;
}

void PaintApp::settle(bool keep_line)
{
    if(!keep_line)
    {
        finish_line();
    }
    m_floating.reset();
    if(m_typing)
    {
        // What was typed stays, without its cursor, as one step.
        show_typing(false);
        auto& open{*m_typing->document};
        const auto typed{!m_typing->text.empty()};
        m_typing.reset();
        if(typed)
        {
            open.history().finish_step("Text", open.document());
            open.set_modified(true);
            refresh_titles();
        }
        else
        {
            open.history().cancel_step();
        }
    }
    if(auto* const view{current_view()})
    {
        view->claim_keys(false);
    }
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

Color PaintApp::ink(bool secondary, bool* see_through)
{
    const auto& color{secondary ? m_color_panel->secondary() : m_color_panel->primary()};
    auto* const open{current()};
    if(see_through != nullptr)
    {
        *see_through = false;
    }
    if(open == nullptr || !open->document().indexed())
    {
        return color;
    }
    const auto& document{open->document()};
    const auto slot{document.layers()[document.active_layer()].palette_slot()};
    const auto& picked{secondary ? m_secondary_index : m_primary_index};
    const auto index{picked ? *picked : document.palettes()->nearest(slot, color)};
    if(see_through != nullptr)
    {
        *see_through = document.palettes()->palettes.at(static_cast<std::size_t>(slot)).transparent == index;
    }
    // The index as the grey of a solid pixel, which exact painting writes as it is.
    const auto grey{index / 255.0};
    return {grey, grey, grey, 1.0};
}

void PaintApp::change_palettes(std::string name, const std::function<void(PaletteSetup&)>& change)
{
    change_layers(std::move(name),
                  [&change](Document& document)
                  {
                      auto setup{*document.palettes()};
                      change(setup);
                      document.set_palettes(std::move(setup));
                  });
}

void PaintApp::register_palette_commands()
{
    auto& commands{m_application.commands()};
    const auto indexed{[this] { return current() != nullptr && current()->document().indexed(); }};
    const auto pick_file{[this](oui::FileDialogMode mode, std::vector<oui::FileFilter> filters,
                                std::string name) -> std::optional<std::filesystem::path>
                         {
                             oui::FileDialogOptions options;
                             options.mode = mode;
                             options.directory = m_last_folder;
                             options.filters = std::move(filters);
                             options.file_name = std::move(name);
                             return oui::file_dialog(m_application, options);
                         }};
    const auto& filters{palette_filters()};
    const auto read_colors{[this, pick_file, filters]() -> std::optional<std::vector<Color>>
                           {
                               const auto path{pick_file(oui::FileDialogMode::open_file, filters, "")};
                               if(!path)
                               {
                                   return std::nullopt;
                               }
                               std::string error;
                               auto bytes{read_file(*path, error)};
                               std::optional<std::vector<Color>> colors;
                               if(bytes)
                               {
                                   colors = read_palette_file(oui::utf8_of(path->filename()), *bytes, error);
                               }
                               if(!colors)
                               {
                                   show_error("Palette", oui::utf8_of(path->filename()) + ": " + error);
                               }
                               return colors;
                           }};
    commands.register_command("palettes.load_setup", "&Load Palette Setup...",
                              [this, indexed, pick_file]
                              {
                                  if(!indexed())
                                  {
                                      return;
                                  }
                                  const auto path{pick_file(oui::FileDialogMode::open_file,
                                                            {{"Palette setup (*.json)", "*.json"}}, "")};
                                  if(!path)
                                  {
                                      return;
                                  }
                                  std::string error;
                                  const auto text{read_file(*path, error)};
                                  std::optional<PaletteSetup> setup;
                                  if(text)
                                  {
                                      setup = read_palette_setup(*text, error);
                                  }
                                  if(!setup)
                                  {
                                      show_error("Palette Setup", oui::utf8_of(path->filename()) + ": " + error);
                                      return;
                                  }
                                  change_palettes("Load Palette Setup", [&setup](PaletteSetup& changed) { changed = *setup; });
                              });
    commands.register_command("palettes.save_setup", "&Save Palette Setup...",
                              [this, indexed, pick_file]
                              {
                                  if(!indexed())
                                  {
                                      return;
                                  }
                                  const auto path{pick_file(oui::FileDialogMode::save_file,
                                                            {{"Palette setup (*.json)", "*.json"}}, "palettes.json")};
                                  std::string error;
                                  if(path && !write_file_atomically(*path, write_palette_setup(*current()->document().palettes()),
                                                                    error))
                                  {
                                      show_error("Palette Setup", error);
                                  }
                              });
    commands.register_command("palettes.load_slot", "Load the Layer's &Palette...",
                              [this, indexed, read_colors]
                              {
                                  if(!indexed())
                                  {
                                      return;
                                  }
                                  const auto colors{read_colors()};
                                  if(!colors)
                                  {
                                      return;
                                  }
                                  const auto& document{current()->document()};
                                  const auto slot{document.layers()[document.active_layer()].palette_slot()};
                                  change_palettes("Load Palette",
                                                  [&](PaletteSetup& setup)
                                                  {
                                                      auto loaded{*colors};
                                                      loaded.resize(static_cast<std::size_t>(setup.size), Color{0, 0, 0, 1});
                                                      setup.palettes.at(static_cast<std::size_t>(slot)).colors = loaded;
                                                  });
                              });
    commands.register_command("palettes.save_slot", "Save the Layer's Pa&lette...",
                              [this, indexed, pick_file, filters]
                              {
                                  if(!indexed())
                                  {
                                      return;
                                  }
                                  const auto& document{current()->document()};
                                  const auto slot{document.layers()[document.active_layer()].palette_slot()};
                                  const auto path{pick_file(oui::FileDialogMode::save_file,
                                                            {filters.begin() + 1, filters.end()},
                                                            "palette.pal")};
                                  std::string error;
                                  if(path && !write_file_atomically(
                                                 *path,
                                                 write_palette_file(oui::utf8_of(path->filename()),
                                                                    document.palettes()->palettes.at(static_cast<std::size_t>(slot)).colors),
                                                 error))
                                  {
                                      show_error("Palette", error);
                                  }
                              });
    commands.register_command("palettes.add", "&Add Palette",
                              [this, indexed]
                              {
                                  if(indexed())
                                  {
                                      change_palettes("Add Palette",
                                                      [](PaletteSetup& setup)
                                                      {
                                                          auto added{PaletteSetup::greys(1, setup.size).palettes.front()};
                                                          added.name = "Palette " + std::to_string(setup.count() + 1);
                                                          setup.palettes.push_back(std::move(added));
                                                      });
                                  }
                              });
    // Moving a palette moves it among the slots, and every layer of the slots it passes shows the other palette: how
    // palette swaps are tried, as reloading a console's palettes.
    const auto move{[this, indexed](int step)
                    {
                        if(!indexed())
                        {
                            return;
                        }
                        const auto& document{current()->document()};
                        const auto slot{document.layers()[document.active_layer()].palette_slot()};
                        const auto other{slot + step};
                        if(other < 0 || other >= document.palettes()->count())
                        {
                            return;
                        }
                        change_palettes(step < 0 ? "Move Palette Up" : "Move Palette Down",
                                        [slot, other](PaletteSetup& setup)
                                        {
                                            std::swap(setup.palettes.at(static_cast<std::size_t>(slot)),
                                                      setup.palettes.at(static_cast<std::size_t>(other)));
                                        });
                    }};
    commands.register_command("palettes.move_up", "Move the Layer's Palette &Up", [move] { move(-1); });
    commands.register_command("palettes.move_down", "Move the Layer's Palette &Down", [move] { move(1); });
    commands.register_command("palettes.load_allowed", "Load Allowed &Colors...",
                              [this, indexed, read_colors]
                              {
                                  if(!indexed())
                                  {
                                      return;
                                  }
                                  if(const auto colors{read_colors()})
                                  {
                                      change_palettes("Allowed Colors",
                                                      [&colors](PaletteSetup& setup) { setup.allowed = *colors; });
                                  }
                              });
    commands.register_command("palettes.clear_allowed", "Allow &Any Color",
                              [this, indexed]
                              {
                                  if(indexed())
                                  {
                                      change_palettes("Allow Any Color", [](PaletteSetup& setup) { setup.allowed.clear(); });
                                  }
                              });
    commands.register_command("image.convert_indexed", "Convert to &Indexed", [this] { convert_to_indexed(); });
    commands.register_command("image.convert_color", "Convert to &Color",
                              [this]
                              {
                                  if(current() == nullptr || !current()->document().indexed())
                                  {
                                      return;
                                  }
                                  change_layers("Convert to Color",
                                                [](Document& document)
                                                {
                                                    for(auto& layer : document.layers())
                                                    {
                                                        auto colored{Layer{PixelFormat::rgba8, layer.name()}};
                                                        colored.set_visible(layer.visible());
                                                        colored.set_opacity(layer.opacity());
                                                        colored.set_blend_mode(layer.blend_mode());
                                                        colored.write(document.colors_of(layer.read(0, 0, document.width(),
                                                                                                    document.height()),
                                                                                         layer.palette_slot()),
                                                                      0, 0);
                                                        layer = std::move(colored);
                                                    }
                                                    document.set_format(PixelFormat::rgba8);
                                                    document.set_palettes(std::nullopt);
                                                });
                              });
}

void PaintApp::convert_to_indexed()
{
    auto* const open{current()};
    if(open == nullptr || open->document().indexed())
    {
        return;
    }
    const auto& document{open->document()};
    // One palette of the image's own colours, index 0 see-through, as many as there are up to 255.
    std::vector<Color> colors;
    std::set<std::uint32_t> seen;
    for(const auto& layer : document.layers())
    {
        const auto pixels{converted(layer.read(0, 0, document.width(), document.height()), PixelFormat::rgba8)};
        for(int y{}; y < pixels.height; ++ y)
        {
            for(int x{}; x < pixels.width; ++ x)
            {
                const auto pixel{pixels.pixel<Rgba8>(x, y)};
                if(pixel.alpha() == 0)
                {
                    continue;
                }
                const auto key{static_cast<std::uint32_t>(pixel.value[0]) << 16u |
                               static_cast<std::uint32_t>(pixel.value[1]) << 8u | pixel.value[2]};
                if(seen.insert(key).second)
                {
                    if(colors.size() == 255)
                    {
                        show_error("Convert to Indexed",
                                   "The image has more than 255 colors. Reduce it first, as with Posterize.");
                        return;
                    }
                    colors.push_back({pixel.value[0] / 255.0, pixel.value[1] / 255.0, pixel.value[2] / 255.0, 1.0});
                }
            }
        }
    }
    PaletteSetup setup;
    setup.size = std::max(2, static_cast<int>((colors.size() + 1 + 15) / 16 * 16));
    Palette palette{"Palette 1", {Color{0, 0, 0, 1}}, 0};
    palette.colors.insert(palette.colors.end(), colors.begin(), colors.end());
    palette.colors.resize(static_cast<std::size_t>(setup.size), Color{0, 0, 0, 1});
    setup.palettes.push_back(std::move(palette));
    change_layers("Convert to Indexed",
                  [&setup](Document& changed)
                  {
                      const auto old_format{changed.format()};
                      changed.set_format(PixelFormat::gray_alpha8);
                      changed.set_palettes(setup);
                      for(auto& layer : changed.layers())
                      {
                          auto indexed{Layer{PixelFormat::gray_alpha8, layer.name()}};
                          indexed.set_visible(layer.visible());
                          indexed.set_opacity(layer.opacity());
                          indexed.set_blend_mode(layer.blend_mode());
                          static_cast<void>(old_format);
                          indexed.write(changed.indices_of(layer.read(0, 0, changed.width(), changed.height()), 0), 0, 0);
                          layer = std::move(indexed);
                      }
                  });
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
    const auto chosen{ask_color(m_application, primary ? "Primary Color" : "Secondary Color",
                                primary ? m_color_panel->primary() : m_color_panel->secondary())};
    if(!chosen)
    {
        return;
    }
    (primary ? m_primary_index : m_secondary_index).reset();
    if(primary)
    {
        m_color_panel->set_primary(*chosen);
    }
    else
    {
        m_color_panel->set_secondary(*chosen);
    }
}

bool PaintApp::edit_text(TextAction action)
{
    auto* const text{dynamic_cast<oui::TextEdit*>(m_application.focused())};
    if(text == nullptr)
    {
        return false;
    }
    std::string taken;
    switch(action)
    {
    case TextAction::undo:
        static_cast<void>(text->undo());
        break;
    case TextAction::redo:
        static_cast<void>(text->redo());
        break;
    case TextAction::cut:
        text->cut_selection(taken);
        break;
    case TextAction::copy:
        text->copy_selection(taken);
        break;
    case TextAction::paste:
        text->paste(m_application.clipboard().text());
        break;
    case TextAction::select_all:
        text->select_all();
        break;
    }
    if(!taken.empty())
    {
        static_cast<void>(m_application.clipboard().set_text(taken));
    }
    return true;
}

PluginWorkbench* PaintApp::workbench_in_tab(std::size_t index)
{
    return m_tabs->content_at<PluginWorkbench>(index);
}

void PaintApp::manage_plugins()
{
    const auto choice{ask_manage_plugins(m_application, m_plugin_records, plugins_folder())};
    switch(choice.action)
    {
    case PluginsChoice::Action::close:
        return;
    case PluginsChoice::Action::new_plugin:
        open_workbench(std::string{new_plugin_source()}, std::nullopt);
        return;
    case PluginsChoice::Action::edit:
    {
        if(choice.plugin >= m_plugin_records.size())
        {
            return;
        }
        const auto& record{m_plugin_records[choice.plugin]};
        // One already open is shown, not opened twice.
        for(std::size_t index{}; record.file && index < m_tabs->child_count(); ++ index)
        {
            if(auto* const open{workbench_in_tab(index)}; open != nullptr && open->file() == record.file)
            {
                m_tabs->select(index);
                return;
            }
        }
        // A built-in plugin opens as a new one to save as one's own: the program's own stay as they are.
        open_workbench(record.source, record.file);
        return;
    }
    case PluginsChoice::Action::reload:
        load_plugins();
        manage_plugins();
        return;
    }
}

void PaintApp::open_workbench(std::string source, std::optional<std::filesystem::path> file)
{
    auto workbench{std::make_unique<PluginWorkbench>(m_application, std::move(source), std::move(file))};
    auto* const shown{workbench.get()};
    shown->on_title_changed([this] { refresh_titles(); });
    shown->on_load_image(
        [this, shown]
        {
            oui::FileDialogOptions options;
            options.mode = oui::FileDialogMode::open_file;
            options.directory = m_last_folder;
            options.filters = image_filters();
            const auto chosen{oui::file_dialog(m_application, options)};
            if(!chosen)
            {
                return;
            }
            std::string error;
            std::optional<Image> image;
            if(const auto bytes{read_file(*chosen, error)})
            {
                if(file_format_of(oui::utf8_of(*chosen)) == FileFormat::openraster)
                {
                    if(const auto layered{read_openraster(*bytes, error)})
                    {
                        image = layered->flatten();
                    }
                }
                else
                {
                    image = read_image(*bytes, error);
                }
            }
            if(!image)
            {
                show_error("Load Image", oui::utf8_of(chosen->filename()) + ": " + error);
                return;
            }
            shown->set_loaded_image(oui::utf8_of(chosen->filename()), std::move(*image));
        });
    auto& added{m_tabs->add_tab(shown->title(), std::move(workbench))};
    m_tabs->select(m_tabs->child_count() - 1);
    refresh_titles();
    show_layers();
    m_application.focus(added.code());
}

bool PaintApp::save_workbench(PluginWorkbench& workbench, bool ask_for_name)
{
    auto file{workbench.file()};
    if(ask_for_name || !file)
    {
        oui::PromptOptions options;
        options.title = "Save Plugin";
        options.label = "Name, saved in the plugins folder:";
        options.text = file ? oui::utf8_of(file->stem()) : std::string{"my_plugin"};
        options.validate = [](std::string_view text) -> std::optional<std::string>
        {
            if(text.empty() || text.find_first_of("/\\:*?\"<>|") != std::string_view::npos)
            {
                return std::string{"A name without / \\ : * ? \" < > |"};
            }
            return std::nullopt;
        };
        const auto name{oui::prompt(m_application, options)};
        if(!name)
        {
            return false;
        }
        auto chosen{oui::path_from_utf8(*name)};
        if(chosen.extension() != ".js")
        {
            chosen += ".js";
        }
        file = plugins_folder() / chosen;
    }
    std::error_code ignored;
    std::filesystem::create_directories(file->parent_path(), ignored);
    std::string error;
    if(!write_file_atomically(*file, workbench.code().text(), error))
    {
        show_error("Save Plugin", error);
        return false;
    }
    workbench.saved(*file);
    // Its effects in the menus as they are now.
    load_plugins();
    return true;
}

std::filesystem::path PaintApp::plugins_folder() const
{
    // A plugin of the person's own is a .js file in the plugins folder beside the settings.
    return m_settings.file_path().parent_path() / "plugins";
}

void PaintApp::load_plugins()
{
    const auto first{m_plugins == nullptr};
    auto& commands{m_application.commands()};
    // The effects' commands of the plugins loaded before go with them.
    if(m_plugins != nullptr)
    {
        for(const auto& effect : m_plugins->effects())
        {
            commands.unregister_command("effect." + std::to_string(effect.index));
        }
    }
    m_effect_values.clear();
    m_last_effect.reset();
    m_system.reset();
    m_plugins = std::make_unique<PluginHost>();
    oui::script::SystemApi::Hooks hooks;
    hooks.print = [](std::string_view text, bool) { std::fwrite(text.data(), 1, text.size(), stderr); };
    hooks.report = [](std::string_view error) { std::fprintf(stderr, "oui_paint: %.*s\n", static_cast<int>(error.size()), error.data()); };
    hooks.has_screen = [] { return true; };
    hooks.title = "OUI Paint";
    m_system = std::make_unique<oui::script::SystemApi>(m_plugins->engine(), m_application, hooks);
    m_plugins->engine().run("globalThis.oui = Object.freeze({...globalThis.__ouiSystem}); delete globalThis.__ouiSystem;",
                            "system.js");
    m_plugin_records.clear();
    const auto load{[this](std::string source, std::string name, std::optional<std::filesystem::path> file)
                    {
                        PluginRecord record{std::move(name), std::move(file), std::move(source), {}, std::nullopt};
                        const auto before{m_plugins->effects().size()};
                        try
                        {
                            m_plugins->load(record.source, record.name);
                        }
                        catch(const std::exception& failure)
                        {
                            record.error = failure.what();
                        }
                        for(auto index{before}; index < m_plugins->effects().size(); ++ index)
                        {
                            record.effects.push_back(m_plugins->effects()[index].name);
                        }
                        m_plugin_records.push_back(std::move(record));
                    }};
    for(const auto& [source, name] : built_in_plugins())
    {
        load(std::string{source}, std::string{name}, std::nullopt);
    }
    const auto folder{plugins_folder()};
    std::error_code ignored;
    std::vector<std::filesystem::path> files;
    for(std::filesystem::directory_iterator entry{folder, ignored}, end; !ignored && entry != end; entry.increment(ignored))
    {
        if(entry->path().extension() == ".js")
        {
            files.push_back(entry->path());
        }
    }
    std::sort(files.begin(), files.end());
    std::string failures;
    for(const auto& file : files)
    {
        std::string error;
        if(const auto source{read_file(file, error)})
        {
            load(*source, oui::utf8_of(file.filename()), file);
            if(const auto& failed{m_plugin_records.back().error})
            {
                failures += oui::utf8_of(file.filename()) + ": " + *failed + "\n";
            }
        }
    }
    // Paint.NET's keys for its adjustments.
    const std::map<std::string, oui::Shortcut> keys{
        {"Auto-Level", {U'l', true, false, true}},           {"Black and White", {U'g', true, false, true}},
        {"Brightness / Contrast", {U't', true, false, true}}, {"Hue / Saturation", {U'u', true, false, true}},
        {"Curves", {U'm', true, false, true}},
        {"Invert Alpha", {U'i', true, true}},                 {"Invert Colors", {U'i', true, false, true}},
        {"Levels", {U'l', true}},                             {"Posterize", {U'p', true, false, true}},
        {"Sepia", {U'e', true, false, true}}};
    std::vector<oui::MenuItem> adjustments;
    std::map<std::string, std::vector<oui::MenuItem>> submenus;
    for(const auto& effect : m_plugins->effects())
    {
        const auto id{"effect." + std::to_string(effect.index)};
        const auto title{effect.parameters.empty() ? effect.name : effect.name + "..."};
        commands.register_command(id, title, [this, index{effect.index}] { run_effect(index, false); });
        if(const auto key{keys.find(effect.name)}; key != keys.end())
        {
            commands.bind(id, key->second);
        }
        if(effect.menu == "Adjustments")
        {
            adjustments.emplace_back(id);
        }
        else
        {
            submenus[effect.menu].emplace_back(id);
        }
    }
    if(first)
    {
        commands.register_command("effects.repeat", "&Repeat Last Effect",
                                  [this]
                                  {
                                      if(m_last_effect)
                                      {
                                          run_effect(*m_last_effect, true);
                                      }
                                  });
        commands.bind("effects.repeat", {U'f', true});
        commands.register_command("effects.manage", "&Manage Plugins...", [this] { manage_plugins(); });
        // Ctrl+Space, as code editors have it: what the plugin's code can use, where its cursor is.
        commands.register_command("edit.suggest", "Suggest",
                                  [this]
                                  {
                                      if(auto* const workbench{workbench_in_tab(m_tabs->selected_index())})
                                      {
                                          workbench->suggest();
                                      }
                                  });
        commands.bind("edit.suggest", oui::KeyShortcut{oui::Key::space, true});
        // The workbench on show hears what each event did, to suggest as one writes.
        m_application.set_event_observer(
            [this](const oui::Event& event)
            {
                if(m_tabs == nullptr)
                {
                    return;
                }
                if(auto* const workbench{workbench_in_tab(m_tabs->selected_index())})
                {
                    workbench->after_event(event);
                }
            });
    }
    std::vector<oui::MenuItem> effects{"effects.repeat", oui::menu_separator()};
    for(auto& [menu, items] : submenus)
    {
        effects.push_back(oui::menu_submenu(menu, std::move(items)));
    }
    effects.push_back(oui::menu_separator());
    effects.emplace_back("effects.manage");
    if(first)
    {
        m_adjustments_menu = m_menu_count;
        add_menu("&Adjustments", std::move(adjustments));
        add_menu("Effe&cts", std::move(effects));
        add_menu("&Help", {"help.documentation", oui::menu_separator(), "help.about"});
        if(!failures.empty())
        {
            show_error("Plugins", "These plugins could not be loaded:\n" + failures);
        }
    }
    else
    {
        m_frame->menu_bar().set_menu_items(m_adjustments_menu, std::move(adjustments));
        m_frame->menu_bar().set_menu_items(m_adjustments_menu + 1, std::move(effects));
    }
}

void PaintApp::show_history_panel(bool shown)
{
    m_history_panel->set_visible(shown);
    m_application.commands().set_checked("view.history_panel", shown);
    m_settings.set_bool_value("history_panel", shown);
    static_cast<void>(m_settings.save());
}

void PaintApp::history_go_to(std::size_t position)
{
    settle();
    if(auto* const open{current()}; open != nullptr && m_stroke == nullptr && position != open->history().position())
    {
        open->refresh(open->history().go_to(open->document(), position));
        open->set_modified(true);
        refresh_titles();
        update_ants();
    }
}

void PaintApp::show_layers_panel(bool shown)
{
    m_frame->right()->set_visible(shown);
    m_application.commands().set_checked("view.layers_panel", shown);
    m_settings.set_bool_value("layers_panel", shown);
    static_cast<void>(m_settings.save());
}

void PaintApp::add_menu(std::string_view title, std::vector<oui::MenuItem> items)
{
    m_frame->menu_bar().add_menu(title, std::move(items));
    ++ m_menu_count;
}

void PaintApp::run_effect(std::size_t index, bool again)
{
    auto* const open{current()};
    if(open == nullptr || m_stroke != nullptr || index >= m_plugins->effects().size())
    {
        return;
    }
    settle();
    const auto effect{m_plugins->effects()[index]};
    auto& document{open->document()};
    auto& layer{document.layers()[document.active_layer()]};
    const auto base{layer};
    const auto& selection{document.selection()};
    const auto area{selection.bounds()};
    if(area.empty())
    {
        return;
    }
    // The effect sees the whole layer, so a blur reaches past the selection's edge, and changes only what is
    // selected, as much as it is.
    // An indexed layer is worked on as its colours and put back as the nearest indices of its palette.
    const auto source{to_work(document.colors_of(layer.read(0, 0, document.width(), document.height()),
                                                 layer.palette_slot()))};
    EffectContext context;
    context.primary = m_color_panel->primary();
    context.secondary = m_color_panel->secondary();
    context.selection_x = area.x;
    context.selection_y = area.y;
    context.selection_width = area.width;
    context.selection_height = area.height;
    auto& values{m_effect_values[index]};
    for(const auto& parameter : effect.parameters)
    {
        values.emplace(parameter.id, parameter.value);
    }
    const auto show{[&](const std::map<std::string, double>& shown)
                    {
                        const auto before{LayersState::of(document)};
                        layer = base;
                        try
                        {
                            const auto result{m_plugins->apply(effect, source, shown, context)};
                            WorkImage inside{area.width, area.height};
                            for(int y{}; y < area.height; ++ y)
                            {
                                for(int x{}; x < area.width; ++ x)
                                {
                                    const auto amount{selection.at(area.x + x, area.y + y) / 255.0f};
                                    const auto* const was{source.at(area.x + x, area.y + y)};
                                    const auto* const made{result.at(area.x + x, area.y + y)};
                                    auto* const out{inside.at(x, y)};
                                    for(int channel{}; channel < 4; ++ channel)
                                    {
                                        out[channel] = was[channel] + (made[channel] - was[channel]) * amount;
                                    }
                                }
                            }
                            if(document.indexed())
                            {
                                // Whole pixels only: half selected or more takes the effect.
                                auto colors{from_work(inside, PixelFormat::rgba8)};
                                auto indices{document.indices_of(colors, layer.palette_slot())};
                                const auto old{layer.read(area.x, area.y, area.width, area.height)};
                                for(int y{}; y < area.height; ++ y)
                                {
                                    for(int x{}; x < area.width; ++ x)
                                    {
                                        if(selection.at(area.x + x, area.y + y) < 128)
                                        {
                                            indices.set_pixel(x, y, old.pixel<GrayAlpha8>(x, y));
                                        }
                                    }
                                }
                                layer.write(indices, area.x, area.y);
                            }
                            else
                            {
                                layer.write(from_work(inside, document.format()), area.x, area.y);
                            }
                        }
                        catch(const std::exception& failure)
                        {
                            open->refresh(changed_tiles(before, LayersState::of(document)));
                            throw;
                        }
                        open->refresh(changed_tiles(before, LayersState::of(document)));
                    }};
    open->history().begin_step(document);
    try
    {
        if(effect.parameters.empty() || again)
        {
            show(values);
        }
        else
        {
            show(values);
            auto failed{false};
            const auto accepted{ask_effect(m_application, effect, values,
                                           [&](const std::map<std::string, double>& changed)
                                           {
                                               if(failed)
                                               {
                                                   return;
                                               }
                                               try
                                               {
                                                   show(changed);
                                               }
                                               catch(const std::exception& failure)
                                               {
                                                   failed = true;
                                                   show_error(effect.name, failure.what());
                                               }
                                           },
                                           histogram_of(source, selection))};
            if(!accepted)
            {
                const auto before{LayersState::of(document)};
                layer = base;
                open->refresh(changed_tiles(before, LayersState::of(document)));
                open->history().cancel_step();
                return;
            }
            show(values);
        }
    }
    catch(const std::exception& failure)
    {
        const auto before{LayersState::of(document)};
        layer = base;
        open->refresh(changed_tiles(before, LayersState::of(document)));
        open->history().cancel_step();
        show_error(effect.name, failure.what());
        return;
    }
    open->history().finish_step(effect.name, document);
    m_last_effect = index;
    open->set_modified(true);
    refresh_titles();
}

void PaintApp::save_all()
{
    const auto selected{m_tabs->selected_index()};
    for(std::size_t index{}; index < m_tabs->child_count(); ++ index)
    {
        const auto* const document{document_in_tab(index)};
        if(document != nullptr && (document->modified() || !document->path()))
        {
            m_tabs->select(index);
            if(!save(false))
            {
                break;
            }
        }
    }
    if(selected < m_tabs->child_count())
    {
        m_tabs->select(selected);
    }
}

void PaintApp::quit()
{
    while(m_tabs->child_count() > 0)
    {
        if(!close(m_tabs->child_count() - 1))
        {
            return;
        }
    }
    m_application.quit();
}

} // namespace oui_paint

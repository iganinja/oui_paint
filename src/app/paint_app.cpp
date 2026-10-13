#include "app/paint_app.h"

#include "app/tools.h"
#include "codecs/codecs.h"
#include "ui/canvas_view.h"
#include "ui/main_frame.h"
#include "ui/new_image_dialog.h"
#include "ui/tool_panel.h"

#include <oui/dialogs.h>
#include <oui/file_dialog.h>
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
        {"Images (*.png, *.jpg, *.bmp, *.tga, *.gif, *.r16)", "*.png *.jpg *.jpeg *.bmp *.tga *.gif *.r16 *.raw"},
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

PaintApp::PaintApp(oui::Application& application) : m_application{application}
{
    std::error_code ignored;
    m_last_folder = std::filesystem::current_path(ignored);
    register_commands();
    build_window();
    m_application.on_close_requested([this] { quit(); });
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
    m_frame->menu_bar().add_menu("&View", {"view.zoom_in", "view.zoom_out", "view.zoom_to_window",
                                           "view.actual_size"});
    auto panel{std::make_unique<ToolPanel>(tool_buttons(), m_icons)};
    m_tool_panel = panel.get();
    m_frame->set_left(std::move(panel));
    auto tabs{std::make_unique<oui::TabView>()};
    m_tabs = tabs.get();
    m_tabs->set_highlight_predicate([this](std::size_t index)
                                    { return index < m_documents.size() && m_documents[index]->modified(); });
    static_cast<void>(m_tabs->on_close_requested([this](std::size_t index) { static_cast<void>(close(index)); }));
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
    m_documents.push_back(std::move(document));
    auto& added{m_tabs->add_tab(m_documents.back()->title(), std::move(view))};
    m_tabs->select(m_documents.size() - 1);
    m_application.focus(added);
    refresh_titles();
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
                               ": name it .png, .jpg, .bmp, .tga or .r16.");
        return false;
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
        options.file_name = path ? path->filename().string() : document->title() + ".png";
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
    return true;
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

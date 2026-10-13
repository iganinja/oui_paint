// OUI Paint: a raster image editor in the spirit of Paint.NET, in a desktop window.
#include "ui/icons.h"
#include "ui/main_frame.h"
#include "ui/tool_panel.h"

#include <oui/graphics/window_backend.h>
#include <oui/label.h>
#include <oui/oui.h>

#include <cstdio>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

using namespace olagarro;

int main(int argument_count, char* const* const arguments)
{
    const oui::CommandLine command_line{oui::program_arguments(argument_count, arguments)};
    if(command_line.has_option("help"))
    {
        std::fputs("OUI Paint\n\n"
                   "Usage: oui_paint [options]\n\n"
                   "Options:\n"
                   "  --help          Show this help text.\n",
                   stdout);
        return 0;
    }

    std::unique_ptr<oui::DesktopWindow> backend;
    try
    {
        oui::WindowOptions window_options;
        window_options.title = "OUI Paint";
        window_options.icon = oui::text_icon("P", {255, 255, 255}, {0, 120, 215});
        backend = oui::window_backend(window_options);
    }
    catch(const std::exception& error)
    {
        std::fprintf(stderr, "oui_paint: cannot open a window: %s\n", error.what());
        return 2;
    }

    oui::Application application{std::move(backend)};
    application.set_theme(oui::classic_theme());
    application.set_title("OUI Paint");
    // Ctrl and the wheel zoom the image, not the interface.
    application.set_wheel_zooms(false);

    auto& commands{application.commands()};
    commands.register_command("file.quit", "E&xit", [&application] { application.quit(); });
    commands.bind("file.quit", {U'q', true});

    oui_paint::Icons icons;
    auto frame{std::make_unique<oui_paint::MainFrame>(commands)};
    frame->menu_bar().add_menu("&File", {"file.quit"});
    frame->set_left(std::make_unique<oui_paint::ToolPanel>(
        std::vector<oui_paint::ToolButton>{
            {"rectangle_select", "RegionEdit", "Rectangle Select (S)"},
            {"move_pixels", "ToolMove", "Move Selected Pixels (M)"},
            {"ellipse_select", "CircleShape2D", "Ellipse Select (S)"},
            {"move_selection", "ToolSelect", "Move Selection (M)"},
            {"zoom", "Zoom", "Zoom (Z)"},
            {"pan", "ToolPan", "Pan (H)"},
            {"bucket", "Bucket", "Paint Bucket (F)"},
            {"gradient", "Gradient", "Gradient (G)"},
            {"brush", "Edit", "Paintbrush (B)"},
            {"eraser", "Eraser", "Eraser (E)"},
            {"pencil", "Edit", "Pencil (P)"},
            {"color_picker", "ColorPick", "Color Picker (K)"},
            {"text", "Label", "Text (T)"},
            {"line", "Line", "Line / Curve (O)"},
            {"shapes", "Rectangle", "Shapes (O)"},
        },
        icons));
    frame->set_center(oui::label(""));
    frame->status_bar().set_left("Ready");

    auto window{oui::window("OUI Paint")};
    window->set_border(oui::WindowBorder::none);
    window->set_close_on_escape(false);
    window->add(std::move(frame));
    application.set_window(std::move(window));
    return application.exec();
}

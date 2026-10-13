// OUI Paint: a raster image editor in the spirit of Paint.NET, in a desktop window.
#include "app/paint_app.h"

#include <oui/graphics/window_backend.h>
#include <oui/oui.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <string>
#include <memory>
#include <utility>

using namespace olagarro;

int main(int argument_count, char* const* const arguments)
{
    const oui::CommandLine command_line{oui::program_arguments(argument_count, arguments)};
    if(command_line.has_option("help"))
    {
        std::fputs("OUI Paint\n\n"
                   "Usage: oui_paint [options] [image...]\n\n"
                   "Opens each image given in a tab of its own.\n\n"
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

    oui_paint::PaintApp paint{application};
    for(std::size_t index{}; const auto path{command_line.positional(index)}; ++ index)
    {
        static_cast<void>(paint.open(std::filesystem::path{std::string{*path}}));
    }
    return application.exec();
}

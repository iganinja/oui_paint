#include "app/window_place.h"

namespace oui_paint
{

olagarro::oui::WindowGeometry window_place_from(const olagarro::oui::Settings& settings)
{
    olagarro::oui::WindowGeometry geometry;
    if(!settings.value("window_x", "").empty() && !settings.value("window_y", "").empty())
    {
        geometry.position = olagarro::oui::Point{static_cast<double>(settings.int_value("window_x", 0)),
                                                 static_cast<double>(settings.int_value("window_y", 0))};
    }
    geometry.size = {
        static_cast<double>(settings.int_value("window_width", olagarro::oui::whole(geometry.size.width))),
        static_cast<double>(settings.int_value("window_height", olagarro::oui::whole(geometry.size.height)))};
    geometry.maximized = settings.bool_value("window_maximized", false);
    return geometry;
}

void keep_window_place(olagarro::oui::Settings& settings, const olagarro::oui::WindowGeometry& geometry)
{
    if(geometry.position)
    {
        settings.set_int_value("window_x", olagarro::oui::whole(geometry.position->x));
        settings.set_int_value("window_y", olagarro::oui::whole(geometry.position->y));
    }
    settings.set_int_value("window_width", olagarro::oui::whole(geometry.size.width));
    settings.set_int_value("window_height", olagarro::oui::whole(geometry.size.height));
    settings.set_bool_value("window_maximized", geometry.maximized);
}

} // namespace oui_paint

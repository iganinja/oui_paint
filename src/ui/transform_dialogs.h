#pragma once

#include "core/document.h"
#include "ui/icons.h"

#include <oui/application.h>

#include <optional>

namespace oui_paint
{

struct ResizeRequest
{
    int width{0};
    int height{0};
    Resampling resampling{Resampling::supersampling};
};

struct CanvasRequest
{
    int width{0};
    int height{0};
    Anchor anchor;
};

struct RotateZoomRequest
{
    double degrees{0.0};
    double zoom{1.0};
    double pan_x{0.0};
    double pan_y{0.0};
};

// Paint.NET's Resize: by a percentage or to a size, keeping the proportions unless told not to.
[[nodiscard]] std::optional<ResizeRequest> ask_resize(olagarro::oui::Application& application, int width, int height);
// Paint.NET's Canvas Size: a new size and where the image goes in it, chosen in a list or on a grid of nine squares.
[[nodiscard]] std::optional<CanvasRequest> ask_canvas_size(olagarro::oui::Application& application, Icons& icons,
                                                           int width, int height);
// Paint.NET's Rotate / Zoom for a layer.
[[nodiscard]] std::optional<RotateZoomRequest> ask_rotate_zoom(olagarro::oui::Application& application);

} // namespace oui_paint

#pragma once

#include "core/layer.h"

#include <oui/application.h>

#include <functional>
#include <optional>
#include <string>

namespace oui_paint
{

struct LayerProperties
{
    std::string name;
    bool visible{true};
    BlendMode blend_mode{BlendMode::normal};
    double opacity{1.0};
};

// Asks for a layer's name, whether it shows, its blend mode and opacity, as Paint.NET's Layer Properties; none when
// cancelled. `changed` hears each change as it is made, so the image shows it.
[[nodiscard]] std::optional<LayerProperties> ask_layer_properties(
    olagarro::oui::Application& application, const LayerProperties& start,
    const std::function<void(const LayerProperties&)>& changed);

} // namespace oui_paint

#pragma once

#include <oui/graphics/svg.h>

#include <map>
#include <string>
#include <string_view>
#include <tuple>

namespace oui_paint
{

// `svg`, a Godot editor icon drawn for a dark interface, in the colours the Godot editor gives it on a light one.
[[nodiscard]] std::string for_light_interface(std::string_view svg);

// The icons built into the program, by name ("Bucket"), drawn for a light interface at the sizes asked and kept.
class Icons
{
  public:
    // `name` drawn as big as fits in `width` by `height` pixels, a whole multiple of its own size where that fits, so it
    // stays sharp, and in the middle; none for a name there is no icon of.
    [[nodiscard]] const olagarro::oui::Picture* picture(std::string_view name, int width, int height);

  private:
    std::map<std::tuple<std::string, int, int>, olagarro::oui::Picture> m_drawn;
};

} // namespace oui_paint

#pragma once

#include <oui/color.h>
#include <oui/graphics/svg.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace oui_paint
{

// `svg`, a Godot editor icon drawn for a dark interface, in the colours the Godot editor gives it on a light one.
[[nodiscard]] std::string for_light_interface(std::string_view svg);

// The icons built into the program, by name ("Bucket"), drawn at the sizes asked and kept: for a light interface, or as
// Godot draws them, for a dark one.
class Icons
{
  public:
    // Whether the interface is dark, as a theme makes it; the icons drawn so far are drawn again.
    void set_dark_interface(bool dark);
    [[nodiscard]] bool dark_interface() const { return m_dark; }
    // `name` drawn as big as fits in `width` by `height` pixels, a whole multiple of its own size where that fits, so it
    // stays sharp, and in the middle; none for a name there is no icon of. Over `background`, when given, light where
    // it is dark and dark where it is light, whatever the interface, as on a list's selected row.
    [[nodiscard]] const olagarro::oui::Picture* picture(std::string_view name, int width, int height,
                                                        std::optional<olagarro::oui::Color> background = std::nullopt);

  private:
    std::map<std::tuple<std::string, int, int, bool>, olagarro::oui::Picture> m_drawn;
    bool m_dark{false};
};

} // namespace oui_paint

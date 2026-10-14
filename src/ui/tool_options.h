#pragma once

#include "app/fonts.h"
#include "tools/fill.h"
#include "tools/gradient.h"
#include "tools/shapes.h"
#include "tools/stroke.h"
#include "tools/text.h"
#include "ui/icons.h"

#include <oui/container.h>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace olagarro::oui
{
class CheckButton;
class ComboBox;
class NumberField;
class Row;
class Slider;
} // namespace olagarro::oui

namespace oui_paint
{

// The bar of the tool in use's options, as Paint.NET's tool bar: the brush's width, hardness and antialiasing, the
// fill's mode, tolerance and sampling, the gradient's kind and mode, a line's arrowheads, a shape's kind and style;
// each tool shows those it uses.
class ToolOptions final : public olagarro::oui::Container
{
  public:
    // Asks for a colour, titled, from `start`, telling `changed` each colour as it is chosen; none when cancelled.
    using ColorChooser = std::function<std::optional<Color>(const std::string& title, const Color& start,
                                                            const std::function<void(const Color&)>& changed)>;

    // Asks where light comes from, in degrees, from `start`, telling `changed` each direction as it is chosen; none
    // when cancelled.
    using LightChooser =
        std::function<std::optional<double>(double start, const std::function<void(double)>& changed)>;

    // `fonts` are those the Text tool offers; `choose_color` asks for the colours of what goes around text, and
    // `choose_light` for where the light that casts its shadow comes from.
    ToolOptions(BrushSettings settings, std::vector<FontFamily> fonts, Icons& icons, ColorChooser choose_color,
                LightChooser choose_light);

    // Shows the options of the tool called `tool`.
    void show_for(std::string_view tool);

    [[nodiscard]] const BrushSettings& settings() const { return m_settings; }
    [[nodiscard]] FillSettings fill() const;
    [[nodiscard]] GradientKind gradient() const;
    [[nodiscard]] bool gradient_transparency() const;
    [[nodiscard]] LineSettings line() const;
    [[nodiscard]] ShapeSettings shape() const;
    // The Text tool's font, none when there are no fonts, and its size in pixels.
    [[nodiscard]] const FontFamily* font() const;
    [[nodiscard]] double font_size() const;
    // What goes around text: its outline's width, its shadow's distance and its glow's reach, and their colours.
    [[nodiscard]] TextEffects text_effects() const;
    // Whether text is bold, italic and struck through.
    struct TextStyle
    {
        bool bold{false};
        bool italic{false};
        bool strikethrough{false};
    };
    [[nodiscard]] TextStyle text_style() const;
    // Width by `step` pixels, kept from 1 to the widest, as [ and ] do.
    void change_width(double step);
    // Called after a brush option changed.
    void on_changed(std::function<void(const BrushSettings&)> changed);
    // Called after what text looks like changed: its font, size, antialiasing or what goes around it.
    void on_text_changed(std::function<void()> changed);

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void arrange(const olagarro::oui::Rect& bounds) override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;

  private:
    void changed();
    void text_changed();
    // Whether the options shown fit in one line `width` columns wide.
    [[nodiscard]] bool one_line(int width) const;

    BrushSettings m_settings;
    olagarro::oui::Row* m_width_group{};
    olagarro::oui::Row* m_hardness_group{};
    olagarro::oui::Row* m_antialias_group{};
    olagarro::oui::Row* m_fill_group{};
    olagarro::oui::Row* m_gradient_group{};
    olagarro::oui::Row* m_line_group{};
    olagarro::oui::Row* m_shape_group{};
    olagarro::oui::Row* m_text_group{};
    // What goes around text: beside the rest where the window is wide enough, on a line of its own under it where not.
    olagarro::oui::Row* m_effects_group{};
    static constexpr int effects_gap{2};
    std::vector<FontFamily> m_fonts;
    olagarro::oui::ComboBox* m_font{};
    olagarro::oui::NumberField* m_font_size{};
    olagarro::oui::NumberField* m_outline{};
    olagarro::oui::NumberField* m_shadow{};
    olagarro::oui::NumberField* m_glow{};
    // The colours of the outline, the shadow and the glow, each shown in a square beside its width.
    std::array<Color, 3> m_effect_colors{Color{1, 1, 1, 1}, Color{0, 0, 0, 0.6}, Color{1, 1, 1, 1}};
    std::array<bool, 3> m_text_style{};
    ColorChooser m_choose_color;
    LightChooser m_choose_light;
    double m_light{135.0};
    olagarro::oui::NumberField* m_width{};
    olagarro::oui::Slider* m_width_slider{};
    olagarro::oui::NumberField* m_hardness{};
    olagarro::oui::CheckButton* m_antialias{};
    olagarro::oui::ComboBox* m_flood_mode{};
    olagarro::oui::NumberField* m_tolerance{};
    olagarro::oui::ComboBox* m_sampling{};
    olagarro::oui::ComboBox* m_gradient_kind{};
    olagarro::oui::ComboBox* m_gradient_mode{};
    olagarro::oui::CheckButton* m_arrow_start{};
    olagarro::oui::CheckButton* m_arrow_end{};
    olagarro::oui::ComboBox* m_shape_kind{};
    olagarro::oui::ComboBox* m_shape_style{};
    std::function<void(const BrushSettings&)> m_changed;
    std::function<void()> m_text_changed;
    bool m_updating{false};
};

} // namespace oui_paint

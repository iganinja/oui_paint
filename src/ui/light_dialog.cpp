#include "ui/light_dialog.h"

#include <oui/button.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/painter.h>
#include <oui/translation.h>
#include <oui/window.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

namespace
{

constexpr double pi{3.14159265358979323846};

// An angle from -180 to 180, as the dialog's field counts it.
[[nodiscard]] double wrapped(double degrees)
{
    auto result{std::fmod(degrees + 180.0, 360.0)};
    if(result < 0.0)
    {
        result += 360.0;
    }
    return result - 180.0;
}

} // namespace

LightDial::LightDial(double degrees) : m_degrees{wrapped(degrees)} {}

void LightDial::set_degrees(double degrees)
{
    m_degrees = wrapped(degrees);
}

void LightDial::on_changed(std::function<void(double degrees)> changed)
{
    m_changed = std::move(changed);
}

oui::Size LightDial::measure(const oui::Constraints& constraints) const
{
    return constraints.constrain({14, 6});
}

void LightDial::paint(oui::Painter& painter, const oui::Theme&) const
{
    m_pixels = painter.pixel_bounds(m_bounds);
    if(!m_pixels || m_pixels->width < 16 || m_pixels->height < 16)
    {
        return;
    }
    constexpr std::uint32_t background{0x202020};
    m_drawing.resize(m_pixels->width, m_pixels->height, background);
    const auto cx{m_pixels->width / 2.0};
    const auto cy{m_pixels->height / 2.0};
    const auto radius{std::min(cx, cy) - 8.0};
    // The ring the sun goes round, and the middle, where the text is.
    const auto around{static_cast<int>(2.0 * pi * radius)};
    for(int step{}; step < around; ++ step)
    {
        const auto angle{2.0 * pi * step / around};
        m_drawing.put(static_cast<int>(std::lround(cx + std::cos(angle) * radius)),
                      static_cast<int>(std::lround(cy + std::sin(angle) * radius)), 0x5A5A56);
    }
    m_drawing.fill(static_cast<int>(cx) - 2, static_cast<int>(cy) - 2, 5, 5, 0xB0B0A8);
    // The light: a line from the sun to the middle, and the sun.
    const auto angle{m_degrees * pi / 180.0};
    const auto sun_x{cx + std::cos(angle) * radius};
    const auto sun_y{cy - std::sin(angle) * radius};
    m_drawing.line(static_cast<int>(std::lround(cx)), static_cast<int>(std::lround(cy)),
                   static_cast<int>(std::lround(sun_x)), static_cast<int>(std::lround(sun_y)), 0xE4C040);
    for(int y{-5}; y <= 5; ++ y)
    {
        for(int x{-5}; x <= 5; ++ x)
        {
            if(x * x + y * y <= 25)
            {
                m_drawing.put(static_cast<int>(std::lround(sun_x)) + x, static_cast<int>(std::lround(sun_y)) + y,
                              0xE4C040);
            }
        }
    }
    painter.draw_image(m_bounds, {m_drawing.pixels.data(), m_drawing.width, m_drawing.height, m_drawing.width}, {});
}

bool LightDial::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pixel || !m_pixels || mouse->right || !(mouse->pressed || mouse->dragging))
    {
        return false;
    }
    const auto dx{mouse->pixel->x - (m_pixels->x + m_pixels->width / 2.0)};
    const auto dy{(m_pixels->y + m_pixels->height / 2.0) - mouse->pixel->y};
    if(dx == 0.0 && dy == 0.0)
    {
        return true;
    }
    auto degrees{std::round(std::atan2(dy, dx) * 180.0 / pi)};
    if(mouse->shift)
    {
        degrees = std::round(degrees / 15.0) * 15.0;
    }
    m_degrees = wrapped(degrees);
    if(m_changed)
    {
        m_changed(m_degrees);
    }
    return true;
}

std::optional<double> ask_light_direction(oui::Application& application, double start,
                                          const std::function<void(double degrees)>& changed)
{
    auto window{oui::window(oui::translate("Dialogs", "Light Direction"))};
    auto& content{oui::column(*window)};
    content.set_gap(oui::Spacing::tight);
    auto& dial{content.add(std::make_unique<LightDial>(start))};
    auto& row{oui::row(content)};
    row.set_gap(oui::Spacing::tight);
    auto& angle_label{oui::label(oui::translate("Dialogs", "&Angle:"), row)};
    auto& angle{oui::number_field(row)};
    angle.set_range(-180, 180);
    angle.set_value(dial.degrees());
    angle_label.set_buddy(angle.text_field());
    oui::label(oui::translate("Dialogs", "degrees"), row);
    static_cast<void>(angle.on_changed(
        [&](double value)
        {
            dial.set_degrees(value);
            changed(dial.degrees());
        }));
    dial.on_changed([&](double degrees) { angle.set_value(degrees); });

    auto& buttons{oui::row(content)};
    buttons.set_gap(oui::Spacing::normal);
    auto& window_reference{*window};
    auto accepted{false};
    oui::button(oui::translate("Dialogs", "OK"), buttons,
                [&]
                {
                    angle.commit();
                    accepted = true;
                    window_reference.close(oui::DialogResult::accepted);
                }).set_default(true);
    oui::button(oui::translate("Dialogs", "Cancel"), buttons,
                [&] { window_reference.close(oui::DialogResult::cancelled); });

    application.focus(angle.text_field());
    static_cast<void>(application.show_modal_and_keep(window));
    if(!accepted)
    {
        return std::nullopt;
    }
    return dial.degrees();
}

} // namespace oui_paint

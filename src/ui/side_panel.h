#pragma once

#include <oui/container.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>
#include <memory>

namespace oui_paint
{

// The colour of the panels at the sides, the tools and the layers: a little darker than the window's, or lighter in a
// dark theme, so they stand apart from it.
[[nodiscard]] inline olagarro::oui::Color side_panel_color(const olagarro::oui::Theme& theme)
{
    const auto& face{theme.status_background};
    const auto light{face.red * 299 + face.green * 587 + face.blue * 114 > 128 * 1000};
    const auto shade{[light](std::uint8_t channel)
                     {
                         return static_cast<std::uint8_t>(light ? channel * 88 / 100
                                                                : std::min(255, channel + (255 - channel) * 12 / 100));
                     }};
    return {shade(face.red), shade(face.green), shade(face.blue)};
}

// A side of the window: what it holds on the panel colour, as tall as the side and as wide as it asks.
class SidePanel final : public olagarro::oui::Container
{
  public:
    explicit SidePanel(std::unique_ptr<olagarro::oui::Widget> content) { m_content = &add(std::move(content)); }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override
    {
        return {m_content->measure(constraints).width, constraints.max_height};
    }
    void arrange(const olagarro::oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        m_content->arrange(bounds);
    }
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override
    {
        painter.fill_rect(bounds(), side_panel_color(theme));
        Container::paint(painter, theme);
    }

  private:
    olagarro::oui::Widget* m_content{};
};

} // namespace oui_paint

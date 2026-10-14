#pragma once

#include "ui/side_panel.h"

#include <oui/container.h>
#include <oui/painter.h>
#include <oui/theme.h>

#include <memory>
#include <string>

namespace oui_paint
{

// A part of a side panel under its title, framed, as the history, the layers or the tools: the title row keeps it
// apart from what is above, the frame from what is round it. Without a title, as where there is no room for one,
// only the frame.
class Section final : public olagarro::oui::Container
{
  public:
    Section(std::string title, std::unique_ptr<olagarro::oui::Widget> content)
        : m_title{std::move(title)}, m_content{&add(std::move(content))}
    {
    }

    [[nodiscard]] olagarro::oui::Widget& content() { return *m_content; }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override
    {
        const auto inner{
            m_content->measure({constraints.max_width, std::max(0, constraints.max_height - title_rows())})};
        // Nothing to show, as the palettes of an image that has none: no title either.
        if(inner.height == 0)
        {
            return {0, 0};
        }
        // As wide as what it holds: the title is short enough for the narrowest, the tools.
        return constraints.constrain({inner.width, inner.height + title_rows()});
    }
    void arrange(const olagarro::oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        m_content->arrange(
            {bounds.x, bounds.y + title_rows(), bounds.width, std::max(0, bounds.height - title_rows())});
    }
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override
    {
        if(m_bounds.height == 0)
        {
            return;
        }
        if(!m_title.empty())
        {
            const auto background{side_panel_color(theme)};
            painter.fill_rect({m_bounds.x, m_bounds.y, m_bounds.width, 1}, background);
            painter.draw_label({m_bounds.x, m_bounds.y}, m_title, {theme.muted_text, background});
        }
        Container::paint(painter, theme);
        if(m_bounds.height > title_rows())
        {
            painter.draw_field_frame(
                {m_bounds.x, m_bounds.y + title_rows(), m_bounds.width, m_bounds.height - title_rows()}, false, theme);
        }
    }

  private:
    [[nodiscard]] int title_rows() const { return m_title.empty() ? 0 : 1; }

    std::string m_title;
    olagarro::oui::Widget* m_content;
};

} // namespace oui_paint

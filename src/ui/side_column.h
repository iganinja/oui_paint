#pragma once

#include <oui/container.h>

#include <algorithm>
#include <memory>

namespace oui_paint
{

// Three widgets one over the other, as the right side holds the history, the layers and the palettes: the lowest as
// tall as it asks, the highest a third of what is left above it, the middle one the rest. A hidden one takes no room.
class SideColumn final : public olagarro::oui::Container
{
  public:
    SideColumn(std::unique_ptr<olagarro::oui::Widget> top, std::unique_ptr<olagarro::oui::Widget> middle,
               std::unique_ptr<olagarro::oui::Widget> bottom)
    {
        m_top = &add(std::move(top));
        m_middle = &add(std::move(middle));
        m_bottom = &add(std::move(bottom));
    }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override
    {
        auto width{0};
        for(const auto* const part : {m_top, m_middle, m_bottom})
        {
            width = std::max(width, part->measure(constraints).width);
        }
        return {width, constraints.max_height};
    }

    void arrange(const olagarro::oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        const auto bottom{
            m_bottom->is_visible() ? std::min(bounds.height, m_bottom->measure({bounds.width, bounds.height}).height)
                                   : 0};
        const auto above{bounds.height - bottom};
        const auto top{m_top->is_visible() ? above / 3 : 0};
        m_top->arrange({bounds.x, bounds.y, bounds.width, top});
        m_middle->arrange({bounds.x, bounds.y + top, bounds.width, above - top});
        m_bottom->arrange({bounds.x, bounds.y + above, bounds.width, bottom});
    }

  private:
    olagarro::oui::Widget* m_top{};
    olagarro::oui::Widget* m_middle{};
    olagarro::oui::Widget* m_bottom{};
};

} // namespace oui_paint

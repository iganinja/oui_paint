#pragma once

#include <oui/container.h>

#include <memory>

namespace oui_paint
{

// Two widgets one over the other: the lower as tall as it asks, the upper filling what is left above it, as the
// palettes under the layers.
class SideColumn final : public olagarro::oui::Container
{
  public:
    SideColumn(std::unique_ptr<olagarro::oui::Widget> upper, std::unique_ptr<olagarro::oui::Widget> lower)
    {
        m_upper = &add(std::move(upper));
        m_lower = &add(std::move(lower));
    }

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override
    {
        const auto upper{m_upper->measure(constraints)};
        const auto lower{m_lower->measure(constraints)};
        return {std::max(upper.width, lower.width), constraints.max_height};
    }

    void arrange(const olagarro::oui::Rect& bounds) override
    {
        Widget::arrange(bounds);
        const auto lower{std::min(bounds.height, m_lower->measure({bounds.width, bounds.height}).height)};
        m_upper->arrange({bounds.x, bounds.y, bounds.width, bounds.height - lower});
        m_lower->arrange({bounds.x, bounds.y + bounds.height - lower, bounds.width, lower});
    }

  private:
    olagarro::oui::Widget* m_upper{};
    olagarro::oui::Widget* m_lower{};
};

} // namespace oui_paint

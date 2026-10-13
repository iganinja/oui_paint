#include "render/viewport.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace oui_paint
{

void Viewport::set_image_size(int width, int height)
{
    m_image_width = std::max(1, width);
    m_image_height = std::max(1, height);
    keep_in_bounds();
}

void Viewport::set_view_size(int width, int height)
{
    // The point in the middle stays in the middle.
    const auto middle{image_at({m_view_width / 2.0, m_view_height / 2.0})};
    m_view_width = std::max(1, width);
    m_view_height = std::max(1, height);
    m_origin = {m_view_width / 2.0 - middle.x * m_zoom, m_view_height / 2.0 - middle.y * m_zoom};
    keep_in_bounds();
}

void Viewport::set_zoom(double zoom, Position anchor)
{
    const auto under{image_at(anchor)};
    m_zoom = std::clamp(zoom, zoom_steps[0], zoom_steps[std::size(zoom_steps) - 1]);
    m_origin = {anchor.x - under.x * m_zoom, anchor.y - under.y * m_zoom};
    keep_in_bounds();
}

void Viewport::zoom_in(Position anchor)
{
    for(const auto step : zoom_steps)
    {
        if(step > m_zoom * 1.0001)
        {
            set_zoom(step, anchor);
            return;
        }
    }
}

void Viewport::zoom_out(Position anchor)
{
    for(auto index{std::size(zoom_steps)}; index > 0; -- index)
    {
        if(zoom_steps[index - 1] < m_zoom * 0.9999)
        {
            set_zoom(zoom_steps[index - 1], anchor);
            return;
        }
    }
}

void Viewport::zoom_to_fit()
{
    auto chosen{zoom_steps[0]};
    for(const auto step : zoom_steps)
    {
        if(step <= 1.0 && m_image_width * step <= m_view_width && m_image_height * step <= m_view_height)
        {
            chosen = step;
        }
    }
    m_zoom = chosen;
    keep_in_bounds();
}

void Viewport::zoom_to(double x, double y, double width, double height)
{
    auto chosen{zoom_steps[0]};
    for(const auto step : zoom_steps)
    {
        if(width * step <= m_view_width && height * step <= m_view_height)
        {
            chosen = step;
        }
    }
    m_zoom = chosen;
    m_origin = {m_view_width / 2.0 - (x + width / 2.0) * m_zoom, m_view_height / 2.0 - (y + height / 2.0) * m_zoom};
    keep_in_bounds();
}

void Viewport::scroll_by(double dx, double dy)
{
    m_origin.x += dx;
    m_origin.y += dy;
    keep_in_bounds();
}

Position Viewport::image_at(Position view) const
{
    return {(view.x - m_origin.x) / m_zoom, (view.y - m_origin.y) / m_zoom};
}

Position Viewport::view_at(Position image) const
{
    return {m_origin.x + image.x * m_zoom, m_origin.y + image.y * m_zoom};
}

void Viewport::keep_in_bounds()
{
    const auto fit{[](double origin, double image, double view)
                   {
                       if(image <= view)
                       {
                           return std::floor((view - image) / 2.0);
                       }
                       return std::clamp(origin, view - image, 0.0);
                   }};
    m_origin.x = fit(m_origin.x, m_image_width * m_zoom, m_view_width);
    m_origin.y = fit(m_origin.y, m_image_height * m_zoom, m_view_height);
}

} // namespace oui_paint

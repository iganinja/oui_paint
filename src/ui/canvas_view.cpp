#include "ui/canvas_view.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <cmath>

using namespace olagarro;

namespace oui_paint
{

CanvasView::CanvasView(OpenDocument& document) : m_document{document}
{
}

void CanvasView::on_changed(std::function<void()> changed)
{
    m_changed = std::move(changed);
}

void CanvasView::changed()
{
    if(m_changed)
    {
        m_changed();
    }
}

Position CanvasView::middle() const
{
    const auto& viewport{m_document.viewport()};
    return {viewport.view_width() / 2.0, viewport.view_height() / 2.0};
}

void CanvasView::zoom_in()
{
    m_document.viewport().zoom_in(middle());
    changed();
}

void CanvasView::zoom_out()
{
    m_document.viewport().zoom_out(middle());
    changed();
}

void CanvasView::zoom_to_fit()
{
    m_document.viewport().zoom_to_fit();
    changed();
}

void CanvasView::actual_size()
{
    m_document.viewport().set_zoom(1.0, middle());
    changed();
}

oui::Size CanvasView::measure(const oui::Constraints& constraints) const
{
    return {constraints.max_width, constraints.max_height};
}

void CanvasView::paint(oui::Painter& painter, const oui::Theme&) const
{
    m_pixels = painter.pixel_bounds(m_bounds);
    if(!m_pixels || m_pixels->width <= 0 || m_pixels->height <= 0)
    {
        return;
    }
    auto& viewport{m_document.viewport()};
    viewport.set_view_size(m_pixels->width, m_pixels->height);
    if(!m_fitted)
    {
        // A document opens as big as fits, never above actual size.
        viewport.zoom_to_fit();
        m_fitted = true;
    }
    m_buffer.resize(static_cast<std::size_t>(m_pixels->width) * m_pixels->height);
    const auto& document{m_document.document()};
    render_view(viewport, m_document.display(), document.width(), document.height(), m_buffer.data(),
                m_pixels->width, m_pixels->height, 0x808080u);
    painter.draw_image(m_bounds, {m_buffer.data(), m_pixels->width, m_pixels->height, m_pixels->width}, {});
}

std::optional<Position> CanvasView::in_view(const oui::MouseEvent& mouse) const
{
    if(!mouse.pixel || !m_pixels)
    {
        return std::nullopt;
    }
    return Position{static_cast<double>(mouse.pixel->x - m_pixels->x) + 0.5,
                    static_cast<double>(mouse.pixel->y - m_pixels->y) + 0.5};
}

bool CanvasView::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr)
    {
        return false;
    }
    const auto at{in_view(*mouse)};
    auto& viewport{m_document.viewport()};
    if(mouse->wheel_delta != 0)
    {
        if(mouse->control && at)
        {
            if(mouse->wheel_delta > 0)
            {
                viewport.zoom_in(*at);
            }
            else
            {
                viewport.zoom_out(*at);
            }
        }
        else
        {
            // Three notches of the wheel go a tenth of the view.
            const auto step{std::max(16.0, viewport.view_height() / 30.0) * mouse->wheel_delta};
            if(mouse->shift)
            {
                viewport.scroll_by(step, 0.0);
            }
            else
            {
                viewport.scroll_by(0.0, step);
            }
        }
        changed();
        return true;
    }
    if(mouse->middle && mouse->pressed && mouse->pixel)
    {
        m_panning_from = *mouse->pixel;
        return true;
    }
    if(mouse->middle && mouse->released)
    {
        m_panning_from.reset();
        return true;
    }
    if(mouse->middle_dragging && m_panning_from && mouse->pixel)
    {
        viewport.scroll_by(mouse->pixel->x - m_panning_from->x, mouse->pixel->y - m_panning_from->y);
        m_panning_from = *mouse->pixel;
        changed();
        return true;
    }
    if(at)
    {
        const auto image{viewport.image_at(*at)};
        const auto& document{m_document.document()};
        if(image.x >= 0.0 && image.y >= 0.0 && image.x < document.width() && image.y < document.height())
        {
            m_pointer = Position{std::floor(image.x), std::floor(image.y)};
        }
        else
        {
            m_pointer.reset();
        }
        changed();
    }
    return mouse->hover || mouse->dragging || mouse->pressed || mouse->released;
}

bool CanvasView::takes_raw_pointer() const
{
    return true;
}

oui::Pointer CanvasView::pointer_at(oui::Point) const
{
    return m_panning_from ? oui::Pointer::grabbing : oui::Pointer::crosshair;
}

bool CanvasView::is_focusable() const
{
    return true;
}

} // namespace oui_paint

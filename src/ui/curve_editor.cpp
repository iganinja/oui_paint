#include "ui/curve_editor.h"

#include <oui/painter.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

namespace
{

// The space round the graph, so the points at its edges can be taken.
constexpr int margin{6};

[[nodiscard]] std::uint32_t mixed(std::uint32_t a, std::uint32_t b, int amount)
{
    const auto channel{[&](int shift)
                       {
                           const auto from{static_cast<int>(a >> shift & 0xFFu)};
                           const auto to{static_cast<int>(b >> shift & 0xFFu)};
                           return static_cast<std::uint32_t>(from + (to - from) * amount / 100) << shift;
                       }};
    return channel(16) | channel(8) | channel(0);
}

} // namespace

CurveEditor::CurveEditor() = default;

void CurveEditor::set_points(std::vector<CurvePoint> points)
{
    m_edit.set_points(std::move(points));
}

oui::Size CurveEditor::measure(const oui::Constraints& constraints) const
{
    // About square, as Paint.NET's, and small enough for a dialog in a small window.
    return constraints.constrain({18, 12});
}

std::optional<std::pair<double, double>> CurveEditor::on_graph(oui::Point pixel) const
{
    if(!m_pixels || m_pixels->width <= 2 * margin + 1 || m_pixels->height <= 2 * margin + 1)
    {
        return std::nullopt;
    }
    const auto side_x{m_pixels->width - 2 * margin - 1};
    const auto side_y{m_pixels->height - 2 * margin - 1};
    return std::pair{static_cast<double>(pixel.x - m_pixels->x - margin) / side_x,
                     1.0 - static_cast<double>(pixel.y - m_pixels->y - margin) / side_y};
}

void CurveEditor::paint(oui::Painter& painter, const oui::Theme&) const
{
    m_pixels = painter.pixel_bounds(m_bounds);
    if(!m_pixels || m_pixels->width <= 2 * margin + 1 || m_pixels->height <= 2 * margin + 1)
    {
        return;
    }
    constexpr std::uint32_t background{0x202020};
    constexpr std::uint32_t grid{0x3A3A38};
    m_drawing.resize(m_pixels->width, m_pixels->height, background);
    const auto side_x{m_pixels->width - 2 * margin - 1};
    const auto side_y{m_pixels->height - 2 * margin - 1};
    const auto at_x{[&](double x) { return margin + static_cast<int>(std::lround(x * side_x)); }};
    const auto at_y{[&](double y) { return margin + static_cast<int>(std::lround((1.0 - y) * side_y)); }};
    // The histogram, as bars from the bottom, the tallest reaching the top.
    if(m_histogram)
    {
        const auto tallest{*std::max_element(m_histogram->begin(), m_histogram->end())};
        for(int column{}; column <= side_x && tallest > 0.0; ++ column)
        {
            const auto level{std::min<std::size_t>(255, static_cast<std::size_t>(column * 256 / (side_x + 1)))};
            const auto height{static_cast<int>(std::lround((*m_histogram)[level] / tallest * side_y))};
            m_drawing.fill(margin + column, margin + side_y - height + 1, 1, height, 0x34342F);
        }
    }
    for(int quarter{}; quarter <= 4; ++ quarter)
    {
        m_drawing.line(at_x(quarter / 4.0), margin, at_x(quarter / 4.0), margin + side_y, grid);
        m_drawing.line(margin, at_y(quarter / 4.0), margin + side_x, at_y(quarter / 4.0), grid);
    }
    const auto draw_curve{[&](const std::vector<CurvePoint>& points, std::uint32_t color)
                          {
                              const auto table{curve_table(points)};
                              auto last_y{at_y(table[0])};
                              for(int column{}; column <= side_x; ++ column)
                              {
                                  const auto place{static_cast<double>(column) / side_x * 255.0};
                                  const auto low{static_cast<std::size_t>(std::floor(place))};
                                  const auto high{std::min<std::size_t>(255, low + 1)};
                                  const auto value{table[low] + (table[high] - table[low]) * (place - low)};
                                  const auto y{at_y(value)};
                                  m_drawing.line(margin + std::max(0, column - 1), last_y, margin + column, y, color);
                                  last_y = y;
                              }
                          }};
    for(const auto& [points, color] : m_faint)
    {
        draw_curve(points, mixed(color, background, 55));
    }
    draw_curve(m_edit.points(), m_color);
    for(std::size_t index{}; index < m_edit.points().size(); ++ index)
    {
        const auto& point{m_edit.points()[index]};
        const auto held{m_edit.held() == index};
        m_drawing.fill(at_x(point.x) - 3, at_y(point.y) - 3, 7, 7, held ? 0xE4A82C : m_color);
        m_drawing.fill(at_x(point.x) - 2, at_y(point.y) - 2, 5, 5, held ? 0xE4A82C : background);
    }
    painter.draw_image(m_bounds, {m_drawing.pixels.data(), m_drawing.width, m_drawing.height, m_drawing.width}, {});
}

bool CurveEditor::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pixel)
    {
        return false;
    }
    const auto place{on_graph(*mouse->pixel)};
    if(!place)
    {
        return false;
    }
    auto changed{false};
    if(mouse->pressed)
    {
        changed = m_edit.press(place->first, place->second, mouse->right);
    }
    else if(mouse->dragging)
    {
        changed = m_edit.drag(place->first, place->second);
    }
    else if(mouse->released)
    {
        m_edit.release();
    }
    else
    {
        return false;
    }
    if(changed && m_changed)
    {
        m_changed();
    }
    return true;
}

} // namespace oui_paint

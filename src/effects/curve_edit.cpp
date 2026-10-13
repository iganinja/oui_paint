#include "effects/curve_edit.h"

#include <algorithm>
#include <cmath>

namespace oui_paint
{

namespace
{

// The closest two points may be, a level apart.
constexpr double gap{1.0 / 255.0};
// How far off the graph a point dragged goes away.
constexpr double off{0.3};

} // namespace

CurveEdit::CurveEdit() : m_points{{0.0, 0.0}, {1.0, 1.0}}
{
}

void CurveEdit::set_points(std::vector<CurvePoint> points)
{
    m_points = std::move(points);
    m_held.reset();
    m_taken.reset();
}

std::optional<std::size_t> CurveEdit::point_near(double x, double y) const
{
    std::optional<std::size_t> nearest;
    auto best{reach};
    for(std::size_t index{}; index < m_points.size(); ++ index)
    {
        const auto distance{std::hypot(m_points[index].x - x, m_points[index].y - y)};
        if(distance <= best)
        {
            best = distance;
            nearest = index;
        }
    }
    return nearest;
}

bool CurveEdit::press(double x, double y, bool right)
{
    x = std::clamp(x, 0.0, 1.0);
    y = std::clamp(y, 0.0, 1.0);
    const auto near{point_near(x, y)};
    if(right)
    {
        if(near && m_points.size() > 2)
        {
            m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(*near));
            return true;
        }
        return false;
    }
    if(near)
    {
        m_held = near;
        return false;
    }
    // A new point where pressed, between those either side, unless as close to one as two points may be.
    const auto after{std::find_if(m_points.begin(), m_points.end(), [x](const CurvePoint& point) { return point.x > x; })};
    if((after != m_points.end() && after->x - x < gap) ||
       (after != m_points.begin() && x - std::prev(after)->x < gap))
    {
        return false;
    }
    // Inserted first: the vector may move, and with it where it begins.
    const auto inserted{m_points.insert(after, {x, y})};
    m_held = static_cast<std::size_t>(std::distance(m_points.begin(), inserted));
    return true;
}

bool CurveEdit::drag(double x, double y)
{
    if(!m_held)
    {
        return false;
    }
    const auto index{*m_held};
    const auto inner{index > 0 && index + 1 < m_points.size()};
    // Far off the graph, an inner point goes; back on it, it comes back.
    const auto away{x < -off || x > 1.0 + off || y < -off || y > 1.0 + off};
    if(inner && away)
    {
        if(!m_taken)
        {
            m_taken = m_points[index];
            m_points.erase(m_points.begin() + static_cast<std::ptrdiff_t>(index));
            return true;
        }
        return false;
    }
    if(m_taken)
    {
        m_points.insert(m_points.begin() + static_cast<std::ptrdiff_t>(index), *m_taken);
        m_taken.reset();
    }
    const auto low{index > 0 ? m_points[index - 1].x + gap : 0.0};
    const auto high{index + 1 < m_points.size() ? m_points[index + 1].x - gap : 1.0};
    const CurvePoint moved{std::clamp(x, low, std::max(low, high)), std::clamp(y, 0.0, 1.0)};
    if(moved == m_points[index])
    {
        return false;
    }
    m_points[index] = moved;
    return true;
}

void CurveEdit::release()
{
    m_held.reset();
    m_taken.reset();
}

} // namespace oui_paint

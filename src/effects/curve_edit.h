#pragma once

#include "effects/curves.h"

#include <optional>
#include <vector>

namespace oui_paint
{

// Editing a curve's points with the pointer, in the curve's own measure, 0 to 1 both ways: a press near a point takes
// it, elsewhere adds one there; dragging moves it between its neighbours; dragging one between others far off the
// graph, or a right press on it, takes it away. The points stay sorted, two at least.
class CurveEdit
{
  public:
    // How near a press must be to a point to take it.
    static constexpr double reach{0.04};

    CurveEdit();

    [[nodiscard]] const std::vector<CurvePoint>& points() const { return m_points; }
    void set_points(std::vector<CurvePoint> points);

    // True when the points changed.
    bool press(double x, double y, bool right);
    bool drag(double x, double y);
    void release();
    // The point being dragged, if any.
    [[nodiscard]] std::optional<std::size_t> held() const { return m_held; }

  private:
    [[nodiscard]] std::optional<std::size_t> point_near(double x, double y) const;

    std::vector<CurvePoint> m_points;
    std::optional<std::size_t> m_held;
    // The point dragged off and taken away, put back if the drag comes back.
    std::optional<CurvePoint> m_taken;
};

} // namespace oui_paint

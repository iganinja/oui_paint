#pragma once

#include "core/selection.h"
#include "effects/pieces.h"

#include <array>
#include <map>
#include <string>
#include <vector>

namespace oui_paint
{

// A point of a curve, as the Curves adjustment edits it: what goes in, x, and what comes out, y, both from 0 to 1.
struct CurvePoint
{
    double x{0.0};
    double y{0.0};

    [[nodiscard]] bool operator==(const CurvePoint& other) const { return x == other.x && y == other.y; }
};

// What each of 256 levels becomes through a curve going through `points`, sorted by x: smooth between them and never
// turning back between two of them, flat past the first and the last.
[[nodiscard]] std::array<double, 256> curve_table(const std::vector<CurvePoint>& points);

// A curve as an effect's values keep it, under a name of one letter, as "l" for luminosity: its points, to edit it
// again, and its table, which the Curves plugin applies, as "l0" to "l255".
void put_curve(std::map<std::string, double>& values, const std::string& name, const std::vector<CurvePoint>& points);
// The points of curve `name` in `values`; the straight line from black to white when there are none.
[[nodiscard]] std::vector<CurvePoint> curve_points(const std::map<std::string, double>& values, const std::string& name);

// How many pixels there are of each level, 0 to 255, in luminosity and in each colour, counted where selected.
struct Histogram
{
    std::array<double, 256> luminosity{};
    std::array<double, 256> red{};
    std::array<double, 256> green{};
    std::array<double, 256> blue{};
};

// Of the image's pixels inside `selection`, each counted as much as it is selected, see-through ones as little as
// they show.
[[nodiscard]] Histogram histogram_of(const WorkImage& image, const Selection& selection);

} // namespace oui_paint

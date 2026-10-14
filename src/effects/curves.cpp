#include "effects/curves.h"

#include <algorithm>
#include <cmath>

namespace oui_paint
{

std::array<double, 256> curve_table(const std::vector<CurvePoint>& points)
{
    std::array<double, 256> table{};
    if(points.empty())
    {
        for(std::size_t level{}; level < table.size(); ++ level)
        {
            table[level] = static_cast<double>(level) / 255.0;
        }
        return table;
    }
    const auto count{points.size()};
    // Fritsch and Carlson's monotone cubic: each point's slope, made gentle enough that no stretch overshoots.
    std::vector<double> slopes(count, 0.0);
    std::vector<double> secants(count > 1 ? count - 1 : 0);
    for(std::size_t index{}; index + 1 < count; ++ index)
    {
        const auto dx{points[index + 1].x - points[index].x};
        secants[index] = dx > 0.0 ? (points[index + 1].y - points[index].y) / dx : 0.0;
    }
    if(count > 1)
    {
        slopes.front() = secants.front();
        slopes.back() = secants.back();
        for(std::size_t index{1}; index + 1 < count; ++ index)
        {
            slopes[index] =
                secants[index - 1] * secants[index] <= 0.0 ? 0.0 : (secants[index - 1] + secants[index]) / 2.0;
        }
        for(std::size_t index{}; index + 1 < count; ++ index)
        {
            if(secants[index] == 0.0)
            {
                slopes[index] = slopes[index + 1] = 0.0;
                continue;
            }
            const auto a{slopes[index] / secants[index]};
            const auto b{slopes[index + 1] / secants[index]};
            const auto length{a * a + b * b};
            if(length > 9.0)
            {
                const auto scale{3.0 / std::sqrt(length)};
                slopes[index] = scale * a * secants[index];
                slopes[index + 1] = scale * b * secants[index];
            }
        }
    }
    for(std::size_t level{}; level < table.size(); ++ level)
    {
        const auto x{static_cast<double>(level) / 255.0};
        double y{};
        if(x <= points.front().x)
        {
            y = points.front().y;
        }
        else if(x >= points.back().x)
        {
            y = points.back().y;
        }
        else
        {
            std::size_t index{};
            while(index + 2 < count && x > points[index + 1].x)
            {
                ++ index;
            }
            const auto& from{points[index]};
            const auto& to{points[index + 1]};
            const auto h{to.x - from.x};
            const auto t{(x - from.x) / h};
            const auto t2{t * t};
            const auto t3{t2 * t};
            y = (2 * t3 - 3 * t2 + 1) * from.y + (t3 - 2 * t2 + t) * h * slopes[index] + (-2 * t3 + 3 * t2) * to.y +
                (t3 - t2) * h * slopes[index + 1];
        }
        table[level] = std::clamp(y, 0.0, 1.0);
    }
    return table;
}

void put_curve(std::map<std::string, double>& values, const std::string& name, const std::vector<CurvePoint>& points)
{
    // What a curve with more points had before goes.
    for(auto at{values.begin()}; at != values.end();)
    {
        at = at->first.rfind(name + "p", 0) == 0 ? values.erase(at) : std::next(at);
    }
    values[name + "pn"] = static_cast<double>(points.size());
    for(std::size_t index{}; index < points.size(); ++ index)
    {
        values[name + "px" + std::to_string(index)] = points[index].x;
        values[name + "py" + std::to_string(index)] = points[index].y;
    }
    const auto table{curve_table(points)};
    for(std::size_t level{}; level < table.size(); ++ level)
    {
        values[name + std::to_string(level)] = table[level];
    }
}

std::vector<CurvePoint> curve_points(const std::map<std::string, double>& values, const std::string& name)
{
    const auto count{values.find(name + "pn")};
    if(count == values.end())
    {
        return {{0.0, 0.0}, {1.0, 1.0}};
    }
    std::vector<CurvePoint> points;
    for(std::size_t index{}; index < static_cast<std::size_t>(count->second); ++ index)
    {
        points.push_back(
            {values.at(name + "px" + std::to_string(index)), values.at(name + "py" + std::to_string(index))});
    }
    return points;
}

Histogram histogram_of(const WorkImage& image, const Selection& selection)
{
    Histogram histogram;
    const auto area{selection.bounds()};
    const auto level{[](float value)
                     { return static_cast<std::size_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f)); }};
    for(int y{area.y}; y < area.y + area.height; ++ y)
    {
        for(int x{area.x}; x < area.x + area.width; ++ x)
        {
            const auto* const pixel{image.at(x, y)};
            const auto weight{static_cast<double>(pixel[3]) * selection.at(x, y) / 255.0};
            if(weight <= 0.0)
            {
                continue;
            }
            // Its colour as it shows, without the alpha the work image multiplies it by.
            const auto red{pixel[0] / pixel[3]};
            const auto green{pixel[1] / pixel[3]};
            const auto blue{pixel[2] / pixel[3]};
            histogram.red[level(red)] += weight;
            histogram.green[level(green)] += weight;
            histogram.blue[level(blue)] += weight;
            histogram.luminosity[level(0.299f * red + 0.587f * green + 0.114f * blue)] += weight;
        }
    }
    return histogram;
}

} // namespace oui_paint

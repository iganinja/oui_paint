#pragma once

#include "core/layer.h"
#include "core/selection.h"
#include "render/viewport.h"

#include <array>
#include <map>
#include <optional>
#include <vector>

namespace oui_paint
{

// A colour as four numbers from 0 to 1, red, green, blue and alpha, whatever the depth it is written at.
struct Color
{
    double red{0.0};
    double green{0.0};
    double blue{0.0};
    double alpha{1.0};

    [[nodiscard]] bool operator==(const Color& other) const
    {
        return red == other.red && green == other.green && blue == other.blue && alpha == other.alpha;
    }
};

struct BrushSettings
{
    // Across, in pixels.
    double width{8.0};
    // How much of the radius is solid before it fades to the edge: 1 is a hard brush, 0 fades from the middle.
    double hardness{0.75};
    bool antialias{true};
};

enum class StrokeMode
{
    paint,
    erase,
};

// A stroke of a brush being drawn on a layer, from the pointer's first position to its last. Each pixel takes the most
// the stroke ever covered it, mixed over the pixel as it was before the stroke: where the stroke crosses itself a
// see-through colour does not build up.
class Stroke
{
  public:
    // Paints only where `selection` selects, as much as it selects.
    Stroke(Layer& layer, const Selection& selection, Color color, BrushSettings settings, StrokeMode mode);

    // Draws from the last point to `point`, in the image's pixels, whose centres are at half pixels.
    void line_to(Position point);
    // The tiles written since last asked.
    [[nodiscard]] std::vector<TilePoint> take_changed();

  private:
    void cover_segment(Position from, Position to);
    // A rectangle of the image's pixels.
    struct Area
    {
        int x;
        int y;
        int width;
        int height;
    };
    // Mixes the stroke into the pixels of the tile at `point` inside `area`.
    void apply(TilePoint point, const Area& area);
    [[nodiscard]] double coverage(double distance) const;

    Layer& m_layer;
    const Selection& m_selection;
    // The layer as it was when the stroke began, what every pixel is mixed over.
    Layer m_original;
    int m_width;
    int m_height;
    Color m_color;
    BrushSettings m_settings;
    StrokeMode m_mode;
    std::optional<Position> m_last;
    std::map<TilePoint, std::vector<float>> m_coverage;
    std::vector<TilePoint> m_changed;
};

// Mixes `color` over the layer wherever `selection` selects, as much as it selects, as Backspace does; returns the
// tiles written.
std::vector<TilePoint> fill_selected(Layer& layer, const Selection& selection, const Color& color);
// Takes away the layer's pixels wherever `selection` selects, as much as it selects, as Delete does.
std::vector<TilePoint> erase_selected(Layer& layer, const Selection& selection);

// The layer's pixels inside the selection's bounds, each as see-through as the selection leaves it, and where they were:
// what Move Selected Pixels lifts.
struct Lifted
{
    Image pixels;
    int x{0};
    int y{0};
};
[[nodiscard]] Lifted lift_selected(const Layer& layer, const Selection& selection);
// Mixes `image`, of the layer's format, over the layer with its top left corner at x, y; returns the tiles written.
std::vector<TilePoint> lay_over(Layer& layer, const Image& image, int x, int y);

} // namespace oui_paint

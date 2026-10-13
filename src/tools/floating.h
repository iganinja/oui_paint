#pragma once

#include "core/layer.h"
#include "core/selection.h"
#include "core/transform.h"
#include "render/viewport.h"

#include <array>
#include <vector>

namespace oui_paint
{

// Pixels lifted off a layer to be moved, turned and scaled before they are put down: what they are, each as
// see-through as the selection left it, how much each was selected, and where they were.
struct FloatingPiece
{
    Image pixels;
    // Grey: how much each pixel was selected; solid.
    Image mask;
    int x{0};
    int y{0};
};

// Where a piece goes from where it was: moved, then turned and scaled round its middle. A negative scale mirrors it.
struct PieceTransform
{
    double dx{0.0};
    double dy{0.0};
    double radians{0.0};
    double scale_x{1.0};
    double scale_y{1.0};
};

// Lifts what `selection` selects of `layer`; all of the layer when nothing is selected.
[[nodiscard]] FloatingPiece lift_piece(const Layer& layer, const Selection& selection);
// From the piece's pixels where it was, to where they go.
[[nodiscard]] Affine piece_forward(const FloatingPiece& piece, const PieceTransform& transform);
// The piece's corners where they go: top left, top right, bottom right, bottom left.
[[nodiscard]] std::array<Position, 4> piece_corners(const FloatingPiece& piece, const PieceTransform& transform);
// Lays the piece, transformed, over `base` into `layer`, and makes the selection its mask where it went. A transform
// that only moves, mirrors or turns by quarter turns keeps every pixel exactly. Returns nothing; the caller redraws.
void place_piece(Layer& layer, const Layer& base, Selection& selection, const FloatingPiece& piece,
                 const PieceTransform& transform);

} // namespace oui_paint

#pragma once

#include "core/document.h"
#include "render/viewport.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace oui_paint
{

// What a document looks like on screen, tile by tile: its visible layers mixed, as 0xAARRGGBB with straight alpha,
// made when first asked for and kept until told the tile changed. Each tile is also kept halved, quartered and so on,
// each level averaged from the one before, so a reduced image is drawn from pixels that sum up the ones they stand for.
class DisplayCache
{
  public:
    static constexpr int levels{9};

    explicit DisplayCache(const Document& document);

    void invalidate_all();
    // The tile at `point` changed, and every reduced tile it is part of.
    void invalidate(TilePoint point);
    // The tile at `point` of `level`, (tile_size >> level) pixels a side, row after row.
    [[nodiscard]] const std::vector<std::uint32_t>& tile(TilePoint point, int level);
    // Makes every tile of `points` at `level` that is not kept, and the levels above it that it is made from, the
    // tiles shared among the cores: tile() then only finds them.
    void prepare(const std::vector<TilePoint>& points, int level);
    // A tile prepare() made, read only, as the cores read it at once.
    [[nodiscard]] const std::vector<std::uint32_t>& prepared(TilePoint point, int level) const
    {
        return m_tiles.at({std::clamp(level, 0, levels - 1), point});
    }

  private:
    [[nodiscard]] std::vector<std::uint32_t> mixed(TilePoint point) const;
    [[nodiscard]] std::vector<std::uint32_t> halved(TilePoint point, int level);
    // A tile halved from the one of the level above, which must be kept.
    [[nodiscard]] std::vector<std::uint32_t> halved_from(const std::vector<std::uint32_t>& above, int level) const;

    const Document& m_document;
    std::map<std::pair<int, TilePoint>, std::vector<std::uint32_t>> m_tiles;
};

// The view of a document as `viewport` places it, in 0x00RRGGBB pixels row after row, `width` by `height`: the image
// over a checkerboard where it is see-through, `background` around it. Sharp pixels from 100% up, reduced levels
// below.
void render_view(const Viewport& viewport, DisplayCache& cache, int image_width, int image_height,
                 std::uint32_t* pixels, int width, int height, std::uint32_t background);

// Where the selection's edge is in a view `width` by `height` as `viewport` places the image: the view's pixels, as
// indices row after row, inside the selection with a neighbour to the right or below that is not.
[[nodiscard]] std::vector<int> selection_edge(const Viewport& viewport, const Selection& selection, int width, int height);
// The edge drawn as marching ants: dashes of black and white that move as `phase` grows.
void draw_marching_ants(const std::vector<int>& edge, std::uint32_t* pixels, int width, int phase);

} // namespace oui_paint

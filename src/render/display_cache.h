#pragma once

#include "core/document.h"
#include "render/viewport.h"

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

  private:
    [[nodiscard]] std::vector<std::uint32_t> mixed(TilePoint point) const;
    [[nodiscard]] std::vector<std::uint32_t> halved(TilePoint point, int level);

    const Document& m_document;
    std::map<std::pair<int, TilePoint>, std::vector<std::uint32_t>> m_tiles;
};

// The view of a document as `viewport` places it, in 0x00RRGGBB pixels row after row, `width` by `height`: the image
// over a checkerboard where it is see-through, `background` around it. Sharp pixels from 100% up, reduced levels
// below.
void render_view(const Viewport& viewport, DisplayCache& cache, int image_width, int image_height,
                 std::uint32_t* pixels, int width, int height, std::uint32_t background);

} // namespace oui_paint

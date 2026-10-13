#include "core/tile.h"

namespace oui_paint
{

Tile::Tile(PixelFormat format)
    : m_format{format}, m_bytes(static_cast<std::size_t>(tile_size) * tile_size * bytes_per_pixel(format))
{
}

} // namespace oui_paint

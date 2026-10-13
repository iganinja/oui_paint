#pragma once

#include "core/pixel_format.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace oui_paint
{

// A layer is kept in squares of this many pixels a side: what undo keeps, what work is shared out by, and what is
// drawn again when something changes.
inline constexpr int tile_size{256};

// Which tile, counted in tiles from the layer's top left corner.
struct TilePoint
{
    int x{0};
    int y{0};

    [[nodiscard]] bool operator==(const TilePoint& other) const { return x == other.x && y == other.y; }
    [[nodiscard]] bool operator<(const TilePoint& other) const { return y != other.y ? y < other.y : x < other.x; }
};

// The pixels of one tile, see-through black when made.
class Tile
{
  public:
    explicit Tile(PixelFormat format);

    [[nodiscard]] PixelFormat format() const { return m_format; }
    [[nodiscard]] std::uint8_t* row(int y) { return &m_bytes[static_cast<std::size_t>(y) * row_bytes()]; }
    [[nodiscard]] const std::uint8_t* row(int y) const { return &m_bytes[static_cast<std::size_t>(y) * row_bytes()]; }
    [[nodiscard]] std::size_t row_bytes() const { return tile_size * bytes_per_pixel(m_format); }

    template <typename PixelType> [[nodiscard]] PixelType pixel(int x, int y) const
    {
        PixelType result;
        std::memcpy(result.value.data(), row(y) + x * sizeof(PixelType), sizeof(PixelType));
        return result;
    }
    template <typename PixelType> void set_pixel(int x, int y, const PixelType& pixel)
    {
        std::memcpy(row(y) + x * sizeof(PixelType), pixel.value.data(), sizeof(PixelType));
    }

  private:
    PixelFormat m_format;
    std::vector<std::uint8_t> m_bytes;
};

} // namespace oui_paint

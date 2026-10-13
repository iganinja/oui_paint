#include "core/layer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace oui_paint
{

namespace
{

// The tile a pixel coordinate is in, also for negative ones.
[[nodiscard]] int tile_of(int coordinate)
{
    return coordinate >= 0 ? coordinate / tile_size : -((-coordinate + tile_size - 1) / tile_size);
}

} // namespace

const std::vector<BlendMode>& all_blend_modes()
{
    static const std::vector<BlendMode> modes{
        BlendMode::normal,     BlendMode::multiply, BlendMode::additive,   BlendMode::color_burn, BlendMode::color_dodge,
        BlendMode::reflect,    BlendMode::glow,     BlendMode::overlay,    BlendMode::difference, BlendMode::negation,
        BlendMode::lighten,    BlendMode::darken,   BlendMode::screen,     BlendMode::xor_};
    return modes;
}

std::string_view blend_mode_name(BlendMode mode)
{
    switch(mode)
    {
    case BlendMode::normal:
        return "Normal";
    case BlendMode::multiply:
        return "Multiply";
    case BlendMode::additive:
        return "Additive";
    case BlendMode::color_burn:
        return "Color Burn";
    case BlendMode::color_dodge:
        return "Color Dodge";
    case BlendMode::reflect:
        return "Reflect";
    case BlendMode::glow:
        return "Glow";
    case BlendMode::overlay:
        return "Overlay";
    case BlendMode::difference:
        return "Difference";
    case BlendMode::negation:
        return "Negation";
    case BlendMode::lighten:
        return "Lighten";
    case BlendMode::darken:
        return "Darken";
    case BlendMode::screen:
        return "Screen";
    case BlendMode::xor_:
        break;
    }
    return "Xor";
}

Layer::Layer(PixelFormat format, std::string name) : m_format{format}, m_name{std::move(name)}
{
}

void Layer::set_opacity(double opacity)
{
    m_opacity = std::isnan(opacity) ? 1.0 : std::clamp(opacity, 0.0, 1.0);
}

std::shared_ptr<const Tile> Layer::tile(TilePoint point) const
{
    const auto found{m_tiles.find(point)};
    return found != m_tiles.end() ? found->second : nullptr;
}

Tile& Layer::writable_tile(TilePoint point)
{
    auto& slot{m_tiles[point]};
    if(slot == nullptr)
    {
        slot = std::make_shared<Tile>(m_format);
    }
    else if(slot.use_count() > 1)
    {
        slot = std::make_shared<Tile>(*slot);
    }
    return *slot;
}

void Layer::set_tile(TilePoint point, std::shared_ptr<const Tile> tile)
{
    if(tile == nullptr)
    {
        m_tiles.erase(point);
        return;
    }
    if(tile->format() != m_format)
    {
        throw std::invalid_argument{"A tile of another format cannot go into this layer."};
    }
    // Never written to while anyone else holds it: writable_tile copies it first.
    m_tiles[point] = std::const_pointer_cast<Tile>(std::move(tile));
}

void Layer::write(const Image& image, int x, int y)
{
    if(image.format != m_format)
    {
        throw std::invalid_argument{"An image of another format cannot be written into this layer."};
    }
    const auto pixel_bytes{bytes_per_pixel(m_format)};
    for(auto tile_y{tile_of(y)}; tile_y <= tile_of(y + image.height - 1); ++ tile_y)
    {
        for(auto tile_x{tile_of(x)}; tile_x <= tile_of(x + image.width - 1); ++ tile_x)
        {
            const auto left{std::max(x, tile_x * tile_size)};
            const auto right{std::min(x + image.width, (tile_x + 1) * tile_size)};
            const auto top{std::max(y, tile_y * tile_size)};
            const auto bottom{std::min(y + image.height, (tile_y + 1) * tile_size)};
            if(left >= right || top >= bottom)
            {
                continue;
            }
            auto& tile{writable_tile({tile_x, tile_y})};
            for(auto row{top}; row < bottom; ++ row)
            {
                std::memcpy(tile.row(row - tile_y * tile_size) + (left - tile_x * tile_size) * pixel_bytes,
                            &image.bytes[static_cast<std::size_t>(row - y) * image.row_bytes() +
                                         static_cast<std::size_t>(left - x) * pixel_bytes],
                            static_cast<std::size_t>(right - left) * pixel_bytes);
            }
        }
    }
}

Image Layer::read(int x, int y, int width, int height) const
{
    Image image{m_format, width, height};
    const auto pixel_bytes{bytes_per_pixel(m_format)};
    for(auto tile_y{tile_of(y)}; tile_y <= tile_of(y + height - 1); ++ tile_y)
    {
        for(auto tile_x{tile_of(x)}; tile_x <= tile_of(x + width - 1); ++ tile_x)
        {
            const auto source{tile({tile_x, tile_y})};
            if(source == nullptr)
            {
                continue;
            }
            const auto left{std::max(x, tile_x * tile_size)};
            const auto right{std::min(x + width, (tile_x + 1) * tile_size)};
            const auto top{std::max(y, tile_y * tile_size)};
            const auto bottom{std::min(y + height, (tile_y + 1) * tile_size)};
            for(auto row{top}; row < bottom; ++ row)
            {
                std::memcpy(&image.bytes[static_cast<std::size_t>(row - y) * image.row_bytes() +
                                         static_cast<std::size_t>(left - x) * pixel_bytes],
                            source->row(row - tile_y * tile_size) + (left - tile_x * tile_size) * pixel_bytes,
                            static_cast<std::size_t>(right - left) * pixel_bytes);
            }
        }
    }
    return image;
}

} // namespace oui_paint

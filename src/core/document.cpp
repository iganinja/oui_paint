#include "core/document.h"

#include <oui/translation.h>

#include <algorithm>
#include <functional>

#include "core/blend.h"

#include <cmath>

#include <stdexcept>

namespace oui_paint
{

Document::Document(int width, int height, PixelFormat format)
    : m_width{width}, m_height{height}, m_format{format}, m_selection{width, height}
{
    if(width <= 0 || height <= 0)
    {
        throw std::invalid_argument{"An image must be at least a pixel wide and a pixel tall."};
    }
}

Document Document::from_image(const Image& image)
{
    Document document{image.width, image.height, image.format};
    document.add_layer(olagarro::oui::translate("Layers", "Background")).write(image, 0, 0);
    return document;
}

void Document::set_active_layer(std::size_t index)
{
    if(index >= m_layers.size())
    {
        throw std::out_of_range{"There is no such layer."};
    }
    m_active_layer = index;
}

Layer& Document::add_layer(std::string name)
{
    m_layers.emplace_back(m_format, std::move(name));
    m_active_layer = m_layers.size() - 1;
    return m_layers.back();
}

Layer& Document::insert_layer_above(std::string name)
{
    const auto at{m_layers.empty() ? 0 : m_active_layer + 1};
    m_layers.emplace(m_layers.begin() + static_cast<std::ptrdiff_t>(at), m_format, std::move(name));
    m_active_layer = at;
    return m_layers[at];
}

void Document::remove_layer(std::size_t index)
{
    if(m_layers.size() <= 1 || index >= m_layers.size())
    {
        return;
    }
    m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
    m_active_layer = index > 0 ? index - 1 : 0;
}

void Document::remove_layers(std::vector<std::size_t> indices)
{
    std::sort(indices.begin(), indices.end(), std::greater<>{});
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    indices.erase(std::remove_if(indices.begin(), indices.end(),
                                 [this](std::size_t index) { return index >= m_layers.size(); }),
                  indices.end());
    if(indices.empty())
    {
        return;
    }
    const auto highest{indices.front()};
    auto removed{std::size_t{}};
    for(const auto index : indices)
    {
        if(m_layers.size() <= 1)
        {
            break;
        }
        m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
        ++ removed;
    }
    // Under the highest removed: as many places down as were removed at or under it, then one more.
    const auto under{highest + 1 > removed ? highest + 1 - removed : std::size_t{1}};
    m_active_layer = std::min(under > 0 ? under - 1 : 0, m_layers.size() - 1);
}

void Document::duplicate_layer(std::size_t index)
{
    if(index >= m_layers.size())
    {
        return;
    }
    auto copy{m_layers[index]};
    copy.set_name(copy.name() + " copy");
    m_layers.insert(m_layers.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(copy));
    m_active_layer = index + 1;
}

void Document::move_layer(std::size_t from, std::size_t to)
{
    if(from >= m_layers.size() || to >= m_layers.size() || from == to)
    {
        return;
    }
    auto moved{std::move(m_layers[from])};
    m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(from));
    m_layers.insert(m_layers.begin() + static_cast<std::ptrdiff_t>(to), std::move(moved));
    if(m_active_layer == from)
    {
        m_active_layer = to;
    }
    else if(from < m_active_layer && to >= m_active_layer)
    {
        -- m_active_layer;
    }
    else if(from > m_active_layer && to <= m_active_layer)
    {
        ++ m_active_layer;
    }
}

void Document::merge_down(std::size_t index)
{
    if(index == 0 || index >= m_layers.size())
    {
        return;
    }
    if(indexed())
    {
        // The two as they show, as colours, back to the nearest of the lower one's palette.
        const auto& above{m_layers[index]};
        auto& below{m_layers[index - 1]};
        auto colors{colors_of(below.read(0, 0, m_width, m_height), below.palette_slot())};
        if(above.visible() && above.opacity() > 0.0)
        {
            const auto over{colors_of(above.read(0, 0, m_width, m_height), above.palette_slot())};
            for(int y{}; y < m_height; ++ y)
            {
                for(int x{}; x < m_width; ++ x)
                {
                    const auto pixel{over.pixel<Rgba8>(x, y)};
                    if(pixel.alpha() != 0)
                    {
                        colors.set_pixel(x, y, blend(colors.pixel<Rgba8>(x, y), pixel, above.opacity(),
                                                     above.blend_mode()));
                    }
                }
            }
        }
        auto merged{below.empty_like()};
        merged.write(indices_of(colors, below.palette_slot()), 0, 0);
        below = std::move(merged);
        m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
        m_active_layer = index - 1;
        return;
    }
    const auto& above{m_layers[index]};
    auto& below{m_layers[index - 1]};
    if(above.visible() && above.opacity() > 0.0)
    {
        visit_format(m_format,
                     [&](auto tag)
                     {
                         using PixelType = typename decltype(tag)::Type;
                         for(const auto& [point, source] : above.tiles())
                         {
                             const auto under{below.tile(point)};
                             auto& target{below.writable_tile(point)};
                             for(int y{}; y < tile_size; ++ y)
                             {
                                 for(int x{}; x < tile_size; ++ x)
                                 {
                                     const auto pixel{source->template pixel<PixelType>(x, y)};
                                     if(pixel.alpha() == 0)
                                     {
                                         continue;
                                     }
                                     const auto base{under != nullptr ? under->template pixel<PixelType>(x, y)
                                                                      : PixelType{}};
                                     target.set_pixel(x, y,
                                                      blend(base, pixel, above.opacity(), above.blend_mode()));
                                 }
                             }
                         }
                     });
    }
    m_layers.erase(m_layers.begin() + static_cast<std::ptrdiff_t>(index));
    m_active_layer = index - 1;
}

void Document::flatten_layers()
{
    if(m_layers.size() <= 1)
    {
        return;
    }
    Layer flat{m_format, m_layers.front().name()};
    flat.set_palette_slot(m_layers.front().palette_slot());
    flat.write(indexed() ? indices_of(flatten(), flat.palette_slot()) : flatten(), 0, 0);
    m_layers.clear();
    m_layers.push_back(std::move(flat));
    m_active_layer = 0;
}

void Document::set_size(int width, int height)
{
    if(width <= 0 || height <= 0)
    {
        throw std::invalid_argument{"An image must be at least a pixel wide and a pixel tall."};
    }
    m_width = width;
    m_height = height;
    auto mask{m_selection.mask()};
    m_selection = Selection{width, height};
    m_selection.set_mask(std::move(mask));
}

void Document::crop_to_selection()
{
    const auto area{m_selection.bounds()};
    if(area.empty())
    {
        return;
    }
    for(auto& layer : m_layers)
    {
        auto pixels{layer.read(area.x, area.y, area.width, area.height)};
        visit_format(m_format,
                     [&](auto tag)
                     {
                         using PixelType = typename decltype(tag)::Type;
                         for(int y{}; y < area.height; ++ y)
                         {
                             for(int x{}; x < area.width; ++ x)
                             {
                                 auto pixel{pixels.pixel<PixelType>(x, y)};
                                 auto& alpha{pixel.value[PixelType::channels - 1]};
                                 alpha = static_cast<typename PixelType::ChannelType>(
                                     (alpha * m_selection.at(area.x + x, area.y + y) + 127u) / 255u);
                                 pixels.set_pixel(x, y, pixel);
                             }
                         }
                     });
        auto cropped{layer.empty_like()};
        cropped.write(pixels, 0, 0);
        layer = std::move(cropped);
    }
    m_selection.clear();
    set_size(area.width, area.height);
}

template <typename Change> void Document::remake(int width, int height, Change change)
{
    for(auto& layer : m_layers)
    {
        auto made{layer.empty_like()};
        made.write(change(layer.read(0, 0, m_width, m_height)), 0, 0);
        layer = std::move(made);
    }
    std::optional<Layer> mask;
    if(const auto& old{m_selection.mask()})
    {
        mask.emplace(PixelFormat::gray_alpha8, "Selection");
        // The selection's grey is how much each pixel is selected; its alpha says nothing, so it is made solid
        // everywhere, and resampling mixes the grey alone.
        auto read{old->read(0, 0, m_width, m_height)};
        for(int y{}; y < read.height; ++ y)
        {
            for(int x{}; x < read.width; ++ x)
            {
                read.set_pixel(x, y, GrayAlpha8{{read.pixel<GrayAlpha8>(x, y).value[0], 255}});
            }
        }
        auto image{change(read)};
        mask->write(image, 0, 0);
    }
    set_size(width, height);
    m_selection.set_mask(std::move(mask));
}

void Document::flip(bool horizontal)
{
    remake(m_width, m_height, [horizontal](const Image& image) { return flipped(image, horizontal); });
}

void Document::rotate(int quarter_turns)
{
    const auto sideways{((quarter_turns % 4) + 4) % 2 == 1};
    remake(sideways ? m_height : m_width, sideways ? m_width : m_height,
           [quarter_turns](const Image& image) { return rotated(image, quarter_turns); });
}

void Document::resize(int width, int height, Resampling resampling)
{
    remake(width, height, [=](const Image& image) { return resized(image, width, height, resampling); });
}

void Document::resize_canvas(int width, int height, Anchor anchor)
{
    const auto dx{(anchor.horizontal + 1) * (width - m_width) / 2};
    const auto dy{(anchor.vertical + 1) * (height - m_height) / 2};
    remake(width, height,
           [=](const Image& image)
           {
               return transformed(image, Affine::translation(-dx, -dy), width, height, Resampling::nearest);
           });
}

void Document::flip_layer(std::size_t index, bool horizontal)
{
    if(index >= m_layers.size())
    {
        return;
    }
    auto& layer{m_layers[index]};
    const auto image{flipped(layer.read(0, 0, m_width, m_height), horizontal)};
    auto made{layer.empty_like()};
    made.write(image, 0, 0);
    layer = std::move(made);
}

void Document::rotate_zoom_layer(std::size_t index, double degrees, double zoom, double pan_x, double pan_y,
                                 Resampling resampling)
{
    if(index >= m_layers.size() || zoom <= 0.0)
    {
        return;
    }
    auto& layer{m_layers[index]};
    const auto center_x{m_width / 2.0};
    const auto center_y{m_height / 2.0};
    constexpr auto pi{3.14159265358979323846};
    // From the layer to where it goes: round the middle, bigger, moved; the image is sampled going back.
    const auto forward{Affine::translation(center_x + pan_x, center_y + pan_y)
                           .after(Affine::scale(zoom, zoom))
                           .after(Affine::rotation(degrees * pi / 180.0))
                           .after(Affine::translation(-center_x, -center_y))};
    const auto image{transformed(layer.read(0, 0, m_width, m_height), forward.inverse(), m_width, m_height,
                                 resampling)};
    auto made{layer.empty_like()};
    made.write(image, 0, 0);
    layer = std::move(made);
}

Image Document::colors_of(const Image& pixels, int slot) const
{
    if(!indexed())
    {
        return pixels;
    }
    Image colors{PixelFormat::rgba8, pixels.width, pixels.height};
    for(int y{}; y < pixels.height; ++ y)
    {
        for(int x{}; x < pixels.width; ++ x)
        {
            const auto pixel{pixels.pixel<GrayAlpha8>(x, y)};
            const auto color{pixel.alpha() == 0 ? Color{0, 0, 0, 0} : m_palettes->color(slot, pixel.value[0])};
            const auto byte{[](double value) { return static_cast<std::uint8_t>(std::lround(value * 255.0)); }};
            colors.set_pixel(x, y, Rgba8{{byte(color.red), byte(color.green), byte(color.blue), byte(color.alpha)}});
        }
    }
    return colors;
}

Image Document::indices_of(const Image& colors, int slot) const
{
    if(!indexed())
    {
        return converted(colors, m_format);
    }
    const auto wide{converted(colors, PixelFormat::rgba16)};
    Image indices{PixelFormat::gray_alpha8, colors.width, colors.height};
    const auto& palette{m_palettes->palettes.at(static_cast<std::size_t>(slot))};
    for(int y{}; y < colors.height; ++ y)
    {
        for(int x{}; x < colors.width; ++ x)
        {
            const auto pixel{wide.pixel<Rgba16>(x, y)};
            const Color color{pixel.value[0] / 65535.0, pixel.value[1] / 65535.0, pixel.value[2] / 65535.0,
                              pixel.value[3] / 65535.0};
            const auto index{m_palettes->nearest(slot, color)};
            const auto see_through{palette.transparent == index};
            indices.set_pixel(x, y, GrayAlpha8{{static_cast<std::uint8_t>(index),
                                                static_cast<std::uint8_t>(see_through ? 0 : 255)}});
        }
    }
    return indices;
}

Image Document::flatten() const
{
    if(indexed())
    {
        // Each layer as the colours of its own palette, mixed as colours.
        Image result{PixelFormat::rgba8, m_width, m_height};
        for(const auto& layer : m_layers)
        {
            if(!layer.visible() || layer.opacity() <= 0.0)
            {
                continue;
            }
            const auto colors{colors_of(layer.read(0, 0, m_width, m_height), layer.palette_slot())};
            for(int y{}; y < m_height; ++ y)
            {
                for(int x{}; x < m_width; ++ x)
                {
                    const auto above{colors.pixel<Rgba8>(x, y)};
                    if(above.alpha() != 0)
                    {
                        result.set_pixel(x, y, blend(result.pixel<Rgba8>(x, y), above, layer.opacity(),
                                                     layer.blend_mode()));
                    }
                }
            }
        }
        return result;
    }
    Image result{m_format, m_width, m_height};
    visit_format(m_format,
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     for(const auto& layer : m_layers)
                     {
                         if(!layer.visible() || layer.opacity() <= 0.0)
                         {
                             continue;
                         }
                         const auto pixels{layer.read(0, 0, m_width, m_height)};
                         for(int y{}; y < m_height; ++ y)
                         {
                             for(int x{}; x < m_width; ++ x)
                             {
                                 const auto above{pixels.pixel<PixelType>(x, y)};
                                 if(above.alpha() == 0)
                                 {
                                     continue;
                                 }
                                 result.set_pixel(x, y,
                                                  blend(result.pixel<PixelType>(x, y), above, layer.opacity(),
                                                        layer.blend_mode()));
                             }
                         }
                     }
                 });
    return result;
}

} // namespace oui_paint

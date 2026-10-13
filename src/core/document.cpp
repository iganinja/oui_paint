#include "core/document.h"

#include "core/blend.h"

#include <stdexcept>

namespace oui_paint
{

Document::Document(int width, int height, PixelFormat format) : m_width{width}, m_height{height}, m_format{format}
{
    if(width <= 0 || height <= 0)
    {
        throw std::invalid_argument{"An image must be at least a pixel wide and a pixel tall."};
    }
}

Document Document::from_image(const Image& image)
{
    Document document{image.width, image.height, image.format};
    document.add_layer("Background").write(image, 0, 0);
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
    flat.write(flatten(), 0, 0);
    m_layers.clear();
    m_layers.push_back(std::move(flat));
    m_active_layer = 0;
}

Image Document::flatten() const
{
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

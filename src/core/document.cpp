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

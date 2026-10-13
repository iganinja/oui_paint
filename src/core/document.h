#pragma once

#include "core/image.h"
#include "core/layer.h"

#include <string>
#include <vector>

namespace oui_paint
{

// An image being edited: its size, its pixel format, which all its layers share, and its layers from the bottom up.
class Document
{
  public:
    Document(int width, int height, PixelFormat format);
    // A document of one layer holding `image`.
    [[nodiscard]] static Document from_image(const Image& image);

    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] PixelFormat format() const { return m_format; }

    [[nodiscard]] std::vector<Layer>& layers() { return m_layers; }
    [[nodiscard]] const std::vector<Layer>& layers() const { return m_layers; }
    [[nodiscard]] std::size_t active_layer() const { return m_active_layer; }
    void set_active_layer(std::size_t index);
    Layer& add_layer(std::string name);

    // What the document looks like: the visible layers mixed from the bottom up, in its format.
    [[nodiscard]] Image flatten() const;

  private:
    int m_width;
    int m_height;
    PixelFormat m_format;
    std::vector<Layer> m_layers;
    std::size_t m_active_layer{0};
};

} // namespace oui_paint

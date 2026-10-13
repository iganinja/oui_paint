#include "app/new_image.h"

namespace oui_paint
{

Document make_document(const NewImage& request)
{
    if(request.indexed)
    {
        // Greys from black to white, index 0 see-through: the background is the white at the end, the darkest grey
        // after the see-through index, or see-through.
        Document document{request.width, request.height, PixelFormat::gray_alpha8};
        document.set_palettes(PaletteSetup::greys(request.palettes, request.palette_size));
        auto& layer{document.add_layer("Background")};
        if(request.background != Background::transparent)
        {
            const auto index{request.background == Background::white ? request.palette_size - 1 : 1};
            Image row{PixelFormat::gray_alpha8, request.width, 1};
            for(int x{}; x < request.width; ++ x)
            {
                row.set_pixel(x, 0, GrayAlpha8{{static_cast<std::uint8_t>(index), 255}});
            }
            for(int y{}; y < request.height; ++ y)
            {
                layer.write(row, 0, y);
            }
        }
        return document;
    }
    const auto format{pixel_format(request.gray, request.bits)};
    Document document{request.width, request.height, format};
    auto& layer{document.add_layer("Background")};
    if(request.background == Background::transparent)
    {
        return document;
    }
    const auto value{request.background == Background::white ? 1.0 : 0.0};
    visit_format(format,
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     PixelType pixel;
                     for(auto& channel : pixel.value)
                     {
                         channel = static_cast<typename PixelType::ChannelType>(value * PixelType::full);
                     }
                     pixel.value[PixelType::channels - 1] = PixelType::full;
                     // A row at a time, so a big image does not need a second copy of itself.
                     Image row{format, request.width, 1};
                     for(int x{}; x < request.width; ++ x)
                     {
                         row.set_pixel(x, 0, pixel);
                     }
                     for(int y{}; y < request.height; ++ y)
                     {
                         layer.write(row, 0, y);
                     }
                 });
    return document;
}

} // namespace oui_paint

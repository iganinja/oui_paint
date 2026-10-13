#include "app/new_image.h"

namespace oui_paint
{

Document make_document(const NewImage& request)
{
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

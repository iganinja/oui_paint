#include "effects/mode_change.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace oui_paint
{

namespace
{

// Every layer's colours one under another, as one image the height of them all.
WorkImage stacked(const Document& document)
{
    const auto width{document.width()};
    const auto height{document.height()};
    const auto& layers{document.layers()};
    WorkImage all{width, height * static_cast<int>(layers.size())};
    for(std::size_t index{}; index < layers.size(); ++ index)
    {
        const auto& layer{layers[index]};
        const auto colors{to_work(document.colors_of(layer.read(0, 0, width, height), layer.palette_slot()))};
        std::copy(colors.pixels.begin(), colors.pixels.end(),
                  all.pixels.begin() + static_cast<std::ptrdiff_t>(index * colors.pixels.size()));
    }
    return all;
}

// The rows of layer `index` in an image stacked as `stacked` makes it.
WorkImage layer_of(const WorkImage& all, int height, std::size_t index)
{
    WorkImage part{all.width, height};
    const auto begin{all.pixels.begin() + static_cast<std::ptrdiff_t>(index * part.pixels.size())};
    std::copy(begin, begin + static_cast<std::ptrdiff_t>(part.pixels.size()), part.pixels.begin());
    return part;
}

// A layer of these pixels in place of `layer`, as it shows otherwise.
Layer replaced(const Layer& layer, PixelFormat format, const Image& pixels)
{
    Layer made{format, layer.name()};
    made.set_visible(layer.visible());
    made.set_opacity(layer.opacity());
    made.set_blend_mode(layer.blend_mode());
    made.write(pixels, 0, 0);
    return made;
}

} // namespace

void index_document(PluginHost& host, const Indexer& indexer, const std::map<std::string, double>& values, int count,
                    const std::optional<std::vector<Color>>& palette, Document& document)
{
    const auto all{stacked(document)};
    auto see_through{false};
    for(std::size_t at{3}; at < all.pixels.size() && !see_through; at += 4)
    {
        see_through = all.pixels[at] < 0.5f;
    }
    const auto room{see_through ? 255 : 256};
    IndexContext context;
    if(palette)
    {
        if(static_cast<int>(palette->size()) > room)
        {
            throw std::runtime_error{"the palette has " + std::to_string(palette->size()) + " colors and there is room for " +
                                     std::to_string(room)};
        }
        context.palette = palette;
        context.count = static_cast<int>(palette->size());
    }
    else
    {
        context.count = std::clamp(count, 1, room);
    }
    const auto indexed{host.index(indexer, all, values, context)};
    const auto first{see_through ? 1 : 0};

    PaletteSetup setup;
    Palette made{"Palette 1", {}, std::nullopt};
    if(see_through)
    {
        made.colors.push_back(Color{0, 0, 0, 1});
        made.transparent = 0;
    }
    made.colors.insert(made.colors.end(), indexed.palette.begin(), indexed.palette.end());
    // Rows of 16, as the palettes panel shows them.
    setup.size = std::clamp(static_cast<int>((made.colors.size() + 15) / 16 * 16), 2, 256);
    made.colors.resize(static_cast<std::size_t>(setup.size), Color{0, 0, 0, 1});
    setup.palettes.push_back(std::move(made));

    const auto width{document.width()};
    const auto height{document.height()};
    for(std::size_t index{}; index < document.layers().size(); ++ index)
    {
        const auto part{layer_of(indexed.indices, height, index)};
        Image indices{PixelFormat::gray_alpha8, width, height};
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < width; ++ x)
            {
                const auto* const pixel{part.at(x, y)};
                const auto shown{pixel[3] >= 0.5f};
                const auto value{shown ? first + static_cast<int>(std::lround(pixel[0] * 255.0f)) : 0};
                indices.set_pixel(x, y, GrayAlpha8{{static_cast<std::uint8_t>(value),
                                                    static_cast<std::uint8_t>(shown ? 255 : 0)}});
            }
        }
        auto& layer{document.layers()[index]};
        layer = replaced(layer, PixelFormat::gray_alpha8, indices);
        layer.set_palette_slot(0);
    }
    document.set_format(PixelFormat::gray_alpha8);
    document.set_palettes(std::move(setup));
}

void convert_document(PluginHost& host, const std::vector<ConversionStep>& steps, PixelFormat format,
                      Document& document)
{
    auto all{stacked(document)};
    for(const auto& step : steps)
    {
        all = host.convert(*step.converter, all, step.values);
    }
    const auto height{document.height()};
    for(std::size_t index{}; index < document.layers().size(); ++ index)
    {
        auto& layer{document.layers()[index]};
        layer = replaced(layer, format, from_work(layer_of(all, height, index), format));
    }
    document.set_format(format);
    document.set_palettes(std::nullopt);
}

std::vector<std::string> conversions_between(PixelFormat from, bool from_indexed, PixelFormat to)
{
    const auto gray{[](PixelFormat format)
                    { return format == PixelFormat::gray_alpha8 || format == PixelFormat::gray_alpha16; }};
    const auto wide{[](PixelFormat format) { return format == PixelFormat::rgba16 || format == PixelFormat::gray_alpha16; }};
    std::vector<std::string> steps;
    if((from_indexed || !gray(from)) && gray(to))
    {
        steps.emplace_back("grayscale");
    }
    if(!from_indexed && wide(from) && !wide(to))
    {
        steps.emplace_back("8 bits");
    }
    return steps;
}

} // namespace oui_paint

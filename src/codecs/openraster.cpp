#include "codecs/openraster.h"

#include "codecs/codecs.h"
#include "codecs/palettes.h"
#include "codecs/zip_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

namespace oui_paint
{

namespace
{

struct ModeName
{
    BlendMode mode;
    std::string_view name;
};

constexpr ModeName mode_names[]{
    {BlendMode::normal, "svg:src-over"},          {BlendMode::multiply, "svg:multiply"},
    {BlendMode::additive, "svg:plus"},            {BlendMode::color_burn, "svg:color-burn"},
    {BlendMode::color_dodge, "svg:color-dodge"},  {BlendMode::reflect, "oui-paint:reflect"},
    {BlendMode::glow, "oui-paint:glow"},          {BlendMode::overlay, "svg:overlay"},
    {BlendMode::difference, "svg:difference"},    {BlendMode::negation, "oui-paint:negation"},
    {BlendMode::lighten, "svg:lighten"},          {BlendMode::darken, "svg:darken"},
    {BlendMode::screen, "svg:screen"},            {BlendMode::xor_, "oui-paint:xor"},
};

[[nodiscard]] std::string_view name_of(BlendMode mode)
{
    for(const auto& entry : mode_names)
    {
        if(entry.mode == mode)
        {
            return entry.name;
        }
    }
    return "svg:src-over";
}

[[nodiscard]] BlendMode mode_of(std::string_view name)
{
    for(const auto& entry : mode_names)
    {
        if(entry.name == name)
        {
            return entry.mode;
        }
    }
    return BlendMode::normal;
}

[[nodiscard]] std::string escaped(std::string_view text)
{
    std::string result;
    for(const auto character : text)
    {
        switch(character)
        {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        default:
            result += character;
        }
    }
    return result;
}

[[nodiscard]] std::string unescaped(std::string_view text)
{
    std::string result;
    for(std::size_t index{}; index < text.size(); ++ index)
    {
        if(text[index] != '&')
        {
            result += text[index];
            continue;
        }
        const auto end{text.find(';', index)};
        if(end == std::string_view::npos)
        {
            result += text[index];
            continue;
        }
        const auto entity{text.substr(index + 1, end - index - 1)};
        if(entity == "amp")
        {
            result += '&';
        }
        else if(entity == "lt")
        {
            result += '<';
        }
        else if(entity == "gt")
        {
            result += '>';
        }
        else if(entity == "quot")
        {
            result += '"';
        }
        else if(entity == "apos")
        {
            result += '\'';
        }
        else
        {
            result += text.substr(index, end - index + 1);
        }
        index = end;
    }
    return result;
}

// The attributes of an XML element written as `<name a="1" b='2'>`, from just after its name.
[[nodiscard]] std::map<std::string, std::string> attributes(std::string_view element)
{
    std::map<std::string, std::string> result;
    std::size_t index{};
    while(index < element.size())
    {
        while(index < element.size() && (element[index] == ' ' || element[index] == '\n' || element[index] == '\t' ||
                                          element[index] == '\r'))
        {
            ++ index;
        }
        const auto equals{element.find('=', index)};
        if(equals == std::string_view::npos)
        {
            break;
        }
        auto name{std::string{element.substr(index, equals - index)}};
        name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
        const auto quote_at{element.find_first_of("\"'", equals)};
        if(quote_at == std::string_view::npos)
        {
            break;
        }
        const auto close{element.find(element[quote_at], quote_at + 1)};
        if(close == std::string_view::npos)
        {
            break;
        }
        result[name] = unescaped(element.substr(quote_at + 1, close - quote_at - 1));
        index = close + 1;
    }
    return result;
}

// Every element `<name ...>` in `xml`, in order, as its attributes.
[[nodiscard]] std::vector<std::map<std::string, std::string>> elements(std::string_view xml, std::string_view name)
{
    std::vector<std::map<std::string, std::string>> result;
    const auto opening{"<" + std::string{name}};
    for(auto at{xml.find(opening)}; at != std::string_view::npos; at = xml.find(opening, at + 1))
    {
        const auto after{at + opening.size()};
        if(after >= xml.size() || (xml[after] != ' ' && xml[after] != '/' && xml[after] != '>' && xml[after] != '\n' &&
                                   xml[after] != '\t' && xml[after] != '\r'))
        {
            continue;
        }
        const auto end{xml.find('>', after)};
        if(end == std::string_view::npos)
        {
            break;
        }
        result.push_back(attributes(xml.substr(after, end - after)));
    }
    return result;
}

[[nodiscard]] double number(const std::map<std::string, std::string>& values, const std::string& key, double fallback)
{
    const auto found{values.find(key)};
    if(found == values.end())
    {
        return fallback;
    }
    char* end{};
    const auto value{std::strtod(found->second.c_str(), &end)};
    return end == found->second.c_str() ? fallback : value;
}

// The image reduced to fit in `side` pixels a side, each pixel the one at its middle.
[[nodiscard]] Image thumbnail(const Image& image, int side)
{
    const auto scale{std::max({1.0, static_cast<double>(image.width) / side, static_cast<double>(image.height) / side})};
    const auto width{std::max(1, static_cast<int>(image.width / scale))};
    const auto height{std::max(1, static_cast<int>(image.height / scale))};
    Image result{image.format, width, height};
    const auto pixel_bytes{bytes_per_pixel(image.format)};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            const auto source_x{std::min(image.width - 1, static_cast<int>((x + 0.5) * scale))};
            const auto source_y{std::min(image.height - 1, static_cast<int>((y + 0.5) * scale))};
            std::memcpy(&result.bytes[static_cast<std::size_t>(y) * result.row_bytes() + x * pixel_bytes],
                        &image.bytes[static_cast<std::size_t>(source_y) * image.row_bytes() + source_x * pixel_bytes],
                        pixel_bytes);
        }
    }
    return result;
}

} // namespace

std::string write_openraster(const Document& document)
{
    std::vector<zip_api::Entry> entries;
    entries.push_back({"mimetype", "image/openraster", true});
    std::string stack{"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"};
    stack += "<image version=\"0.0.6\" w=\"" + std::to_string(document.width()) + "\" h=\"" +
             std::to_string(document.height()) + "\">\n  <stack>\n";
    const auto& layers{document.layers()};
    std::vector<zip_api::Entry> pictures;
    for(auto index{layers.size()}; index > 0; -- index)
    {
        const auto& layer{layers[index - 1]};
        const auto source{"data/layer" + std::to_string(index - 1) + ".png"};
        char opacity[32];
        std::snprintf(opacity, sizeof opacity, "%.6g", layer.opacity());
        stack += "    <layer name=\"" + escaped(layer.name()) + "\" src=\"" + source + "\" x=\"0\" y=\"0\" opacity=\"" +
                 opacity + "\" visibility=\"" + (layer.visible() ? "visible" : "hidden") + "\" composite-op=\"" +
                 std::string{name_of(layer.blend_mode())} + "\"" +
                 (document.indexed() ? " oui-paint:palette-slot=\"" + std::to_string(layer.palette_slot()) + "\"" : "") +
                 "/>\n";
        if(document.indexed())
        {
            // A PNG with the palette of its slot, which other programs show in its colours.
            Document alone{document.width(), document.height(), PixelFormat::gray_alpha8};
            PaletteSetup own;
            own.size = document.palettes()->size;
            own.palettes.push_back(document.palettes()->palettes.at(static_cast<std::size_t>(layer.palette_slot())));
            alone.set_palettes(own);
            alone.add_layer(layer.name()).write(layer.read(0, 0, document.width(), document.height()), 0, 0);
            pictures.push_back({source, write_indexed_png(alone), true});
            continue;
        }
        pictures.push_back(
            {source, write_image(layer.read(0, 0, document.width(), document.height()), FileFormat::png), true});
    }
    stack += "  </stack>\n</image>\n";
    entries.push_back({"stack.xml", stack, false});
    if(document.indexed())
    {
        entries.push_back({"oui-paint/palettes.json", write_palette_setup(*document.palettes()), false});
    }
    for(auto& picture : pictures)
    {
        entries.push_back(std::move(picture));
    }
    const auto merged{document.flatten()};
    entries.push_back({"mergedimage.png", write_image(merged, FileFormat::png), true});
    entries.push_back({"Thumbnails/thumbnail.png",
                       write_image(converted(thumbnail(merged, 256), pixel_format(is_gray(merged.format), 8)),
                                   FileFormat::png),
                       true});
    auto file{zip_api::zip(entries)};
    if(!file)
    {
        throw std::runtime_error{"OpenRaster: the zip could not be made."};
    }
    return *file;
}

std::optional<Document> read_openraster(std::string_view bytes, std::string& error)
{
    const auto mimetype{zip_api::unzip(bytes, "mimetype")};
    if(!mimetype || mimetype->rfind("image/openraster", 0) != 0)
    {
        error = "not an OpenRaster file";
        return std::nullopt;
    }
    const auto stack{zip_api::unzip(bytes, "stack.xml")};
    if(!stack)
    {
        error = "an OpenRaster file without stack.xml";
        return std::nullopt;
    }
    const auto images{elements(*stack, "image")};
    if(images.empty())
    {
        error = "stack.xml names no image";
        return std::nullopt;
    }
    const auto width{static_cast<int>(number(images.front(), "w", 0))};
    const auto height{static_cast<int>(number(images.front(), "h", 0))};
    if(width <= 0 || height <= 0)
    {
        error = "stack.xml gives the image no size";
        return std::nullopt;
    }
    struct Read
    {
        std::map<std::string, std::string> values;
        Image image;
    };
    std::vector<Read> read;
    auto bits{8};
    auto gray{true};
    std::optional<PaletteSetup> palettes;
    if(const auto setup_json{zip_api::unzip(bytes, "oui-paint/palettes.json")})
    {
        palettes = read_palette_setup(*setup_json, error);
        if(!palettes)
        {
            return std::nullopt;
        }
    }
    for(const auto& values : elements(*stack, "layer"))
    {
        const auto source{values.find("src")};
        if(source == values.end())
        {
            continue;
        }
        const auto png{zip_api::unzip(bytes, source->second)};
        if(!png)
        {
            error = "the OpenRaster file lacks " + source->second;
            return std::nullopt;
        }
        std::optional<Image> image;
        if(palettes)
        {
            // Its indices as they are.
            if(auto alone{read_indexed_png(*png, error)})
            {
                image = alone->layers().front().read(0, 0, alone->width(), alone->height());
            }
        }
        else
        {
            image = read_image(*png, error);
        }
        if(!image)
        {
            error = source->second + ": " + error;
            return std::nullopt;
        }
        bits = std::max(bits, bits_per_channel(image->format));
        gray = gray && is_gray(image->format);
        read.push_back({values, std::move(*image)});
    }
    Document document{width, height, palettes ? PixelFormat::gray_alpha8 : pixel_format(gray, bits)};
    document.set_palettes(palettes);
    // stack.xml lists the top layer first.
    for(auto index{read.size()}; index > 0; -- index)
    {
        const auto& [values, image]{read[index - 1]};
        const auto name{values.find("name")};
        auto& layer{document.add_layer(name != values.end() ? name->second : "Layer")};
        layer.write(palettes ? image : converted(image, document.format()), static_cast<int>(number(values, "x", 0)),
                    static_cast<int>(number(values, "y", 0)));
        layer.set_opacity(number(values, "opacity", 1.0));
        const auto visibility{values.find("visibility")};
        layer.set_visible(visibility == values.end() || visibility->second != "hidden");
        if(const auto slot{values.find("oui-paint:palette-slot")}; slot != values.end() && palettes)
        {
            layer.set_palette_slot(std::clamp(static_cast<int>(number(values, "oui-paint:palette-slot", 0)), 0,
                                              palettes->count() - 1));
        }
        const auto mode{values.find("composite-op")};
        layer.set_blend_mode(mode != values.end() ? mode_of(mode->second) : BlendMode::normal);
    }
    if(document.layers().empty())
    {
        document.add_layer("Background");
    }
    return document;
}

} // namespace oui_paint

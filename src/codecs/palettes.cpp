#include "codecs/palettes.h"

#include <oui/translation.h>

#include "codecs/codecs.h"
#include "core/json.h"

#include <lodepng.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>

namespace oui_paint
{

namespace
{

[[nodiscard]] std::string lower_extension(std::string_view name)
{
    const auto dot{name.rfind('.')};
    std::string extension{dot == std::string_view::npos ? std::string_view{} : name.substr(dot + 1)};
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension;
}

[[nodiscard]] Color from_bytes(int red, int green, int blue, int alpha = 255)
{
    return {red / 255.0, green / 255.0, blue / 255.0, alpha / 255.0};
}

[[nodiscard]] int byte(double value)
{
    return static_cast<int>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
}

// Lines of text, three numbers at the start of each that has them.
[[nodiscard]] std::vector<Color> numbered_lines(std::string_view text, std::size_t skip_lines)
{
    std::vector<Color> colors;
    std::istringstream lines{std::string{text}};
    std::string line;
    for(std::size_t index{}; std::getline(lines, line); ++ index)
    {
        if(index < skip_lines || line.empty() || line[0] == '#')
        {
            continue;
        }
        std::istringstream numbers{line};
        int red{};
        int green{};
        int blue{};
        if(numbers >> red >> green >> blue)
        {
            colors.push_back(from_bytes(red, green, blue));
        }
    }
    return colors;
}

} // namespace

bool is_paletted_png(std::string_view bytes)
{
    lodepng::State state;
    unsigned width{};
    unsigned height{};
    if(lodepng_inspect(&width, &height, &state, reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()) != 0)
    {
        return false;
    }
    return state.info_png.color.colortype == LCT_PALETTE;
}

std::optional<std::vector<Color>> read_palette_file(std::string_view name, std::string_view bytes, std::string& error)
{
    const auto extension{lower_extension(name)};
    if(extension == "pal" && bytes.substr(0, 8) == "JASC-PAL")
    {
        return numbered_lines(bytes, 3);
    }
    if(extension == "gpl" && bytes.substr(0, 12) == "GIMP Palette")
    {
        return numbered_lines(bytes, 1);
    }
    if(extension == "act" && bytes.size() >= 768)
    {
        // 256 colours, then perhaps how many of them count.
        auto count{256u};
        if(bytes.size() >= 772)
        {
            count = static_cast<unsigned char>(bytes[768]) << 8u | static_cast<unsigned char>(bytes[769]);
            count = std::clamp(count, 1u, 256u);
        }
        std::vector<Color> colors;
        for(unsigned index{}; index < count; ++ index)
        {
            colors.push_back(from_bytes(static_cast<unsigned char>(bytes[index * 3]),
                                        static_cast<unsigned char>(bytes[index * 3 + 1]),
                                        static_cast<unsigned char>(bytes[index * 3 + 2])));
        }
        return colors;
    }
    if(is_paletted_png(bytes))
    {
        // The palette comes with the image itself, not in its header.
        lodepng::State state;
        state.decoder.color_convert = 0;
        std::vector<unsigned char> ignored;
        unsigned width{};
        unsigned height{};
        static_cast<void>(lodepng::decode(ignored, width, height, state,
                                          reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size()));
        std::vector<Color> colors;
        for(std::size_t index{}; index < state.info_png.color.palettesize; ++ index)
        {
            const auto* const entry{&state.info_png.color.palette[index * 4]};
            colors.push_back(from_bytes(entry[0], entry[1], entry[2], entry[3]));
        }
        return colors;
    }
    // Any other image: its distinct colours as they come, row by row.
    auto image{read_image(bytes, error)};
    if(!image)
    {
        error = olagarro::oui::translate("Files", "neither a palette file nor an image");
        return std::nullopt;
    }
    const auto colors8{converted(*image, PixelFormat::rgba8)};
    std::vector<Color> colors;
    std::set<std::uint32_t> seen;
    for(int y{}; y < colors8.height && colors.size() < 256; ++ y)
    {
        for(int x{}; x < colors8.width && colors.size() < 256; ++ x)
        {
            const auto pixel{colors8.pixel<Rgba8>(x, y)};
            if(pixel.alpha() == 0)
            {
                continue;
            }
            const auto key{static_cast<std::uint32_t>(pixel.value[0]) << 16u |
                           static_cast<std::uint32_t>(pixel.value[1]) << 8u | pixel.value[2]};
            if(seen.insert(key).second)
            {
                colors.push_back(from_bytes(pixel.value[0], pixel.value[1], pixel.value[2]));
            }
        }
    }
    return colors;
}

std::string write_palette_file(std::string_view name, const std::vector<Color>& colors)
{
    const auto extension{lower_extension(name)};
    std::string text;
    if(extension == "act")
    {
        std::string bytes(772, '\0');
        for(std::size_t index{}; index < std::min<std::size_t>(colors.size(), 256); ++ index)
        {
            bytes[index * 3] = static_cast<char>(byte(colors[index].red));
            bytes[index * 3 + 1] = static_cast<char>(byte(colors[index].green));
            bytes[index * 3 + 2] = static_cast<char>(byte(colors[index].blue));
        }
        const auto count{std::min<std::size_t>(colors.size(), 256)};
        bytes[768] = static_cast<char>(count >> 8u);
        bytes[769] = static_cast<char>(count & 0xffu);
        bytes[770] = static_cast<char>(0xff);
        bytes[771] = static_cast<char>(0xff);
        return bytes;
    }
    const auto line{[](const Color& color)
                    {
                        return std::to_string(byte(color.red)) + " " + std::to_string(byte(color.green)) + " " +
                               std::to_string(byte(color.blue));
                    }};
    if(extension == "gpl")
    {
        text = "GIMP Palette\nName: OUI Paint\nColumns: 16\n#\n";
        for(const auto& color : colors)
        {
            text += line(color) + "\n";
        }
        return text;
    }
    text = "JASC-PAL\n0100\n" + std::to_string(colors.size()) + "\n";
    for(const auto& color : colors)
    {
        text += line(color) + "\n";
    }
    return text;
}

std::string write_palette_setup(const PaletteSetup& setup)
{
    std::string json{"{\n  \"size\": " + std::to_string(setup.size) + ",\n  \"palettes\": ["};
    for(std::size_t slot{}; slot < setup.palettes.size(); ++ slot)
    {
        const auto& palette{setup.palettes[slot]};
        json += slot == 0 ? "\n" : ",\n";
        json += "    {\"name\": " + json_quoted(palette.name) + ", \"transparent\": " +
                (palette.transparent ? std::to_string(*palette.transparent) : std::string{"null"}) + ", \"colors\": [";
        for(std::size_t index{}; index < palette.colors.size(); ++ index)
        {
            json += (index == 0 ? "" : ", ") + json_quoted(to_hex(palette.colors[index]));
        }
        json += "]}";
    }
    json += "\n  ],\n  \"allowed\": [";
    for(std::size_t index{}; index < setup.allowed.size(); ++ index)
    {
        json += (index == 0 ? "" : ", ") + json_quoted(to_hex(setup.allowed[index]));
    }
    return json + "]\n}\n";
}

std::optional<PaletteSetup> read_palette_setup(std::string_view text, std::string& error)
{
    const auto json{parse_json(text)};
    if(!json || json->kind != Json::Kind::object)
    {
        error = olagarro::oui::translate("Files", "not a palette setup");
        return std::nullopt;
    }
    PaletteSetup setup;
    setup.size = static_cast<int>((*json)["size"].number);
    if(setup.size <= 0 || setup.size > 256)
    {
        error = olagarro::oui::translate("Files", "a palette holds 1 to 256 colours");
        return std::nullopt;
    }
    const auto colors_of{[&error](const Json& list) -> std::optional<std::vector<Color>>
                         {
                             std::vector<Color> colors;
                             for(const auto& item : list.items)
                             {
                                 const auto color{from_hex(item.text)};
                                 if(!color)
                                 {
                                     error = olagarro::oui::format_text(olagarro::oui::translate("Files", "not a colour: {0}"), {item.text});
                                     return std::nullopt;
                                 }
                                 colors.push_back(*color);
                             }
                             return colors;
                         }};
    for(const auto& item : (*json)["palettes"].items)
    {
        Palette palette;
        palette.name = item["name"].text;
        palette.transparent = item["transparent"].kind == Json::Kind::number
                                  ? std::optional<int>{static_cast<int>(item["transparent"].number)}
                                  : std::nullopt;
        auto colors{colors_of(item["colors"])};
        if(!colors)
        {
            return std::nullopt;
        }
        colors->resize(static_cast<std::size_t>(setup.size), Color{0, 0, 0, 1});
        palette.colors = std::move(*colors);
        setup.palettes.push_back(std::move(palette));
    }
    auto allowed{colors_of((*json)["allowed"])};
    if(!allowed)
    {
        return std::nullopt;
    }
    setup.allowed = std::move(*allowed);
    if(setup.palettes.empty())
    {
        error = olagarro::oui::translate("Files", "a palette setup has at least one palette");
        return std::nullopt;
    }
    return setup;
}

std::string write_indexed_png(const Document& document)
{
    const auto& setup{*document.palettes()};
    const auto entries{setup.count() * setup.size};
    if(entries > 256)
    {
        throw std::runtime_error{olagarro::oui::format_text(olagarro::oui::translate("Files", "An indexed PNG holds 256 colours; these palettes have {0}."), {std::to_string(entries)})};
    }
    lodepng::State state;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = 8;
    state.encoder.auto_convert = 0;
    for(int slot{}; slot < setup.count(); ++ slot)
    {
        const auto& palette{setup.palettes[static_cast<std::size_t>(slot)]};
        for(int index{}; index < setup.size; ++ index)
        {
            const auto color{index < static_cast<int>(palette.colors.size()) ? palette.colors[static_cast<std::size_t>(index)]
                                                                             : Color{}};
            const auto alpha{palette.transparent == index ? 0 : byte(color.alpha)};
            for(auto* const info : {&state.info_raw, &state.info_png.color})
            {
                lodepng_palette_add(info, static_cast<unsigned char>(byte(color.red)),
                                    static_cast<unsigned char>(byte(color.green)),
                                    static_cast<unsigned char>(byte(color.blue)), static_cast<unsigned char>(alpha));
            }
        }
    }
    // Where no layer shows, the see-through index of the first palette.
    const auto nothing{setup.palettes.front().transparent.value_or(0)};
    std::vector<unsigned char> indices(static_cast<std::size_t>(document.width()) * document.height(),
                                       static_cast<unsigned char>(nothing));
    std::vector<bool> placed(indices.size(), false);
    const auto& layers{document.layers()};
    for(auto layer{layers.rbegin()}; layer != layers.rend(); ++ layer)
    {
        if(!layer->visible())
        {
            continue;
        }
        const auto pixels{layer->read(0, 0, document.width(), document.height())};
        for(int y{}; y < document.height(); ++ y)
        {
            for(int x{}; x < document.width(); ++ x)
            {
                const auto at{static_cast<std::size_t>(y) * document.width() + x};
                const auto pixel{pixels.pixel<GrayAlpha8>(x, y)};
                if(placed[at] || pixel.alpha() == 0)
                {
                    continue;
                }
                indices[at] = static_cast<unsigned char>(layer->palette_slot() * setup.size + pixel.value[0]);
                placed[at] = true;
            }
        }
    }
    std::vector<unsigned char> file;
    const auto failure{lodepng::encode(file, indices, static_cast<unsigned>(document.width()),
                                       static_cast<unsigned>(document.height()), state)};
    if(failure != 0)
    {
        throw std::runtime_error{std::string{"PNG: "} + lodepng_error_text(failure)};
    }
    return {file.begin(), file.end()};
}

std::optional<Document> read_indexed_png(std::string_view bytes, std::string& error)
{
    lodepng::State state;
    state.decoder.color_convert = 0;
    std::vector<unsigned char> indices;
    unsigned width{};
    unsigned height{};
    const auto failure{lodepng::decode(indices, width, height, state,
                                       reinterpret_cast<const unsigned char*>(bytes.data()), bytes.size())};
    if(failure != 0 || state.info_png.color.colortype != LCT_PALETTE || state.info_png.color.bitdepth != 8)
    {
        error = failure != 0 ? lodepng_error_text(failure) : "not a PNG with a palette of 8 bits";
        return std::nullopt;
    }
    Palette palette{olagarro::oui::format_text(olagarro::oui::translate("Dialogs", "Palette {0}"), {"1"}), {}, std::nullopt};
    for(std::size_t index{}; index < state.info_png.color.palettesize; ++ index)
    {
        const auto* const entry{&state.info_png.color.palette[index * 4]};
        palette.colors.push_back(from_bytes(entry[0], entry[1], entry[2]));
        if(entry[3] == 0 && !palette.transparent)
        {
            palette.transparent = static_cast<int>(index);
        }
    }
    PaletteSetup setup;
    setup.size = static_cast<int>(palette.colors.size());
    setup.palettes.push_back(palette);
    Document document{static_cast<int>(width), static_cast<int>(height), PixelFormat::gray_alpha8};
    document.set_palettes(setup);
    Image image{PixelFormat::gray_alpha8, static_cast<int>(width), static_cast<int>(height)};
    for(unsigned y{}; y < height; ++ y)
    {
        for(unsigned x{}; x < width; ++ x)
        {
            const auto index{indices[static_cast<std::size_t>(y) * width + x]};
            image.set_pixel(static_cast<int>(x), static_cast<int>(y),
                            GrayAlpha8{{index, static_cast<std::uint8_t>(palette.transparent == index ? 0 : 255)}});
        }
    }
    document.add_layer(olagarro::oui::translate("Layers", "Background")).write(image, 0, 0);
    return document;
}

} // namespace oui_paint

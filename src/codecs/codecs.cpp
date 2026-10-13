#include "codecs/codecs.h"

#include "codecs/stb_api.h"

#include <lodepng.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace oui_paint
{

namespace
{

[[nodiscard]] std::string lower(std::string_view text)
{
    std::string result{text};
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return result;
}

// The image's 8-bit pixels with their alpha, or for a file without it, mixed over white where they are see-through.
[[nodiscard]] std::vector<std::uint8_t> channels8(const Image& image, bool keep_alpha)
{
    const auto in{channel_count(image.format)};
    const auto out{keep_alpha ? in : in - 1};
    std::vector<std::uint8_t> result(static_cast<std::size_t>(image.width) * image.height * out);
    std::size_t at{};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            const auto* const pixel{&image.bytes[static_cast<std::size_t>(y) * image.row_bytes() +
                                                 static_cast<std::size_t>(x) * in]};
            const unsigned alpha{pixel[in - 1]};
            for(int channel{}; channel < out; ++ channel)
            {
                result[at++] = keep_alpha ? pixel[channel]
                                          : static_cast<std::uint8_t>((pixel[channel] * alpha + 255u * (255u - alpha) +
                                                                       127u) /
                                                                      255u);
            }
        }
    }
    return result;
}

[[nodiscard]] std::string write_png(const Image& image)
{
    const auto gray{is_gray(image.format)};
    const auto opaque{image.is_opaque()};
    const auto sixteen{bits_per_channel(image.format) == 16};
    const auto in{channel_count(image.format)};
    const auto out{opaque ? in - 1 : in};
    const auto channel_bytes{sixteen ? 2 : 1};
    // PNG keeps 16-bit values with their high byte first.
    std::vector<unsigned char> raw(static_cast<std::size_t>(image.width) * image.height * out * channel_bytes);
    std::size_t at{};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            for(int channel{}; channel < out; ++ channel)
            {
                const auto offset{static_cast<std::size_t>(y) * image.row_bytes() +
                                  (static_cast<std::size_t>(x) * in + channel) * channel_bytes};
                if(sixteen)
                {
                    std::uint16_t value{};
                    std::memcpy(&value, &image.bytes[offset], 2);
                    raw[at++] = static_cast<unsigned char>(value >> 8u);
                    raw[at++] = static_cast<unsigned char>(value & 0xffu);
                }
                else
                {
                    raw[at++] = image.bytes[offset];
                }
            }
        }
    }
    lodepng::State state;
    const auto type{gray ? (opaque ? LCT_GREY : LCT_GREY_ALPHA) : (opaque ? LCT_RGB : LCT_RGBA)};
    state.info_raw.colortype = type;
    state.info_raw.bitdepth = sixteen ? 16u : 8u;
    state.info_png.color.colortype = type;
    state.info_png.color.bitdepth = sixteen ? 16u : 8u;
    state.encoder.auto_convert = 0;
    std::vector<unsigned char> file;
    const auto failure{lodepng::encode(file, raw, static_cast<unsigned>(image.width),
                                       static_cast<unsigned>(image.height), state)};
    if(failure != 0)
    {
        throw std::runtime_error{std::string{"PNG: "} + lodepng_error_text(failure)};
    }
    return {file.begin(), file.end()};
}

} // namespace

std::optional<FileFormat> file_format_of(std::string_view path)
{
    const auto dot{path.rfind('.')};
    if(dot == std::string_view::npos)
    {
        return std::nullopt;
    }
    const auto extension{lower(path.substr(dot + 1))};
    if(extension == "png")
    {
        return FileFormat::png;
    }
    if(extension == "jpg" || extension == "jpeg")
    {
        return FileFormat::jpeg;
    }
    if(extension == "bmp")
    {
        return FileFormat::bmp;
    }
    if(extension == "tga")
    {
        return FileFormat::tga;
    }
    if(extension == "gif")
    {
        return FileFormat::gif;
    }
    if(extension == "r16" || extension == "raw")
    {
        return FileFormat::raw16;
    }
    return std::nullopt;
}

bool can_write(FileFormat format)
{
    return format != FileFormat::gif;
}

PixelFormat written_format(FileFormat file, PixelFormat pixels)
{
    switch(file)
    {
    case FileFormat::png:
        return pixels;
    case FileFormat::raw16:
        return PixelFormat::gray_alpha16;
    default:
        return pixel_format(is_gray(pixels), 8);
    }
}

bool keeps_depth(FileFormat file, PixelFormat pixels)
{
    return written_format(file, pixels) == pixels;
}

std::optional<Image> read_image(std::string_view bytes, std::string& error)
{
    int channels{};
    bool sixteen{};
    if(!stb_api::inspect(bytes, channels, sixteen))
    {
        error = "not an image this program can read";
        return std::nullopt;
    }
    const auto gray{channels <= 2};
    stb_api::Decoded decoded;
    if(!stb_api::decode(bytes, gray ? 2 : 4, decoded, error))
    {
        return std::nullopt;
    }
    Image image{pixel_format(gray, decoded.sixteen_bits ? 16 : 8), decoded.width, decoded.height};
    image.bytes = std::move(decoded.pixels);
    return image;
}

std::optional<Image> read_raw16(std::string_view bytes, int width, int height)
{
    if(width <= 0 || height <= 0 || bytes.size() != static_cast<std::size_t>(width) * height * 2u)
    {
        return std::nullopt;
    }
    Image image{PixelFormat::gray_alpha16, width, height};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            const auto at{(static_cast<std::size_t>(y) * width + x) * 2u};
            const auto value{static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[at]) |
                                                        static_cast<unsigned char>(bytes[at + 1]) << 8u)};
            image.set_pixel(x, y, GrayAlpha16{{value, GrayAlpha16::full}});
        }
    }
    return image;
}

std::optional<std::pair<int, int>> guess_raw16_size(std::size_t byte_count)
{
    if(byte_count == 0 || byte_count % 2 != 0)
    {
        return std::nullopt;
    }
    const auto values{byte_count / 2};
    const auto side{static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(values))))};
    if(side * side == values)
    {
        return std::make_pair(static_cast<int>(side), static_cast<int>(side));
    }
    return std::nullopt;
}

std::string write_image(const Image& image, FileFormat format, int quality)
{
    if(!can_write(format) || !keeps_depth(format, image.format))
    {
        throw std::invalid_argument{"The image must be converted to what the file holds before it is written."};
    }
    switch(format)
    {
    case FileFormat::png:
        return write_png(image);
    case FileFormat::raw16:
    {
        std::string file(static_cast<std::size_t>(image.width) * image.height * 2u, '\0');
        for(int y{}; y < image.height; ++ y)
        {
            for(int x{}; x < image.width; ++ x)
            {
                const auto value{image.pixel<GrayAlpha16>(x, y).value[0]};
                const auto at{(static_cast<std::size_t>(y) * image.width + x) * 2u};
                file[at] = static_cast<char>(value & 0xffu);
                file[at + 1] = static_cast<char>(value >> 8u);
            }
        }
        return file;
    }
    case FileFormat::jpeg:
    {
        const auto pixels{channels8(image, false)};
        return stb_api::encode_jpeg(pixels.data(), image.width, image.height, channel_count(image.format) - 1,
                                    std::clamp(quality, 1, 100));
    }
    case FileFormat::bmp:
    {
        const auto pixels{channels8(image, false)};
        return stb_api::encode_bmp(pixels.data(), image.width, image.height, channel_count(image.format) - 1);
    }
    case FileFormat::tga:
    {
        const auto pixels{channels8(image, true)};
        return stb_api::encode_tga(pixels.data(), image.width, image.height, channel_count(image.format));
    }
    case FileFormat::gif:
        break;
    }
    throw std::invalid_argument{"GIF files are only read."};
}

} // namespace oui_paint

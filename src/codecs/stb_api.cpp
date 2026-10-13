#include "codecs/stb_api.h"

#include <cstring>

#define STBI_NO_STDIO
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

namespace oui_paint::stb_api
{

bool inspect(std::string_view bytes, int& channels, bool& sixteen_bits)
{
    const auto* const data{reinterpret_cast<const stbi_uc*>(bytes.data())};
    const auto size{static_cast<int>(bytes.size())};
    int width{};
    int height{};
    if(stbi_info_from_memory(data, size, &width, &height, &channels) == 0)
    {
        return false;
    }
    sixteen_bits = stbi_is_16_bit_from_memory(data, size) != 0;
    return true;
}

bool decode(std::string_view bytes, int channels_out, Decoded& decoded, std::string& error)
{
    const auto* const data{reinterpret_cast<const stbi_uc*>(bytes.data())};
    const auto size{static_cast<int>(bytes.size())};
    decoded.sixteen_bits = stbi_is_16_bit_from_memory(data, size) != 0;
    void* pixels{};
    if(decoded.sixteen_bits)
    {
        pixels = stbi_load_16_from_memory(data, size, &decoded.width, &decoded.height, &decoded.channels,
                                          channels_out);
    }
    else
    {
        pixels = stbi_load_from_memory(data, size, &decoded.width, &decoded.height, &decoded.channels, channels_out);
    }
    if(pixels == nullptr)
    {
        error = stbi_failure_reason() != nullptr ? stbi_failure_reason() : "unknown error";
        return false;
    }
    const auto byte_count{static_cast<std::size_t>(decoded.width) * decoded.height * channels_out *
                          (decoded.sixteen_bits ? 2u : 1u)};
    decoded.pixels.resize(byte_count);
    std::memcpy(decoded.pixels.data(), pixels, byte_count);
    stbi_image_free(pixels);
    return true;
}

namespace
{

void append(void* context, void* data, int size)
{
    static_cast<std::string*>(context)->append(static_cast<const char*>(data), static_cast<std::size_t>(size));
}

} // namespace

std::string encode_jpeg(const std::uint8_t* pixels, int width, int height, int channels, int quality)
{
    std::string file;
    stbi_write_jpg_to_func(append, &file, width, height, channels, pixels, quality);
    return file;
}

std::string encode_bmp(const std::uint8_t* pixels, int width, int height, int channels)
{
    std::string file;
    stbi_write_bmp_to_func(append, &file, width, height, channels, pixels);
    return file;
}

std::string encode_tga(const std::uint8_t* pixels, int width, int height, int channels)
{
    std::string file;
    stbi_write_tga_to_func(append, &file, width, height, channels, pixels);
    return file;
}

} // namespace oui_paint::stb_api

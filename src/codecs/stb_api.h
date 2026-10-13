#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// stb_image and stb_image_write behind a door of OUI Paint's own, so their warnings stay in the one file that holds
// them.
namespace oui_paint::stb_api
{

struct Decoded
{
    int width{0};
    int height{0};
    // 1 grey, 2 grey and alpha, 3 colour, 4 colour and alpha, as the file has them.
    int channels{0};
    bool sixteen_bits{false};
    // The pixels in `channels_out` channels each, 8 or 16 bits as `sixteen_bits` says.
    std::vector<std::uint8_t> pixels;
};

// Decodes the file in `bytes` into `channels_out` channels; false, with stb's reason in `error`, when it cannot.
[[nodiscard]] bool decode(std::string_view bytes, int channels_out, Decoded& decoded, std::string& error);
// Asks a file for its channels and depth without decoding it.
[[nodiscard]] bool inspect(std::string_view bytes, int& channels, bool& sixteen_bits);

// 8-bit pixels, `channels` of them each, as a file.
[[nodiscard]] std::string encode_jpeg(const std::uint8_t* pixels, int width, int height, int channels, int quality);
[[nodiscard]] std::string encode_bmp(const std::uint8_t* pixels, int width, int height, int channels);
[[nodiscard]] std::string encode_tga(const std::uint8_t* pixels, int width, int height, int channels);

} // namespace oui_paint::stb_api

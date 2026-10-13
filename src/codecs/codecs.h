#pragma once

#include "core/image.h"

#include <optional>
#include <string>
#include <string_view>

namespace oui_paint
{

// The kinds of file images are read from and written to.
enum class FileFormat
{
    png,
    jpeg,
    bmp,
    tga,
    gif,
    // 16-bit grey values, little-endian, row after row, with no header: a heightmap.
    raw16,
    // Layers and all: openraster.h reads and writes it.
    openraster,
};

// The format a file name's extension says, none for one it does not know.
[[nodiscard]] std::optional<FileFormat> file_format_of(std::string_view path);
// Whether the format can be written, and whether it holds pixels of `format` as they are: depth and colour.
[[nodiscard]] bool can_write(FileFormat format);
[[nodiscard]] bool keeps_depth(FileFormat file, PixelFormat pixels);
// The pixel format it is written in from `pixels`: the same where it holds it, 8 bits or grey where it does not.
[[nodiscard]] PixelFormat written_format(FileFormat file, PixelFormat pixels);

// An image from the bytes of a PNG, JPEG, BMP, TGA or GIF (its first frame), at the depth it has, grey where it is
// grey; none, with what went wrong in `error`, when it cannot be read.
[[nodiscard]] std::optional<Image> read_image(std::string_view bytes, std::string& error);
// A raw 16-bit heightmap of `width` by `height`; none when the bytes are not that many.
[[nodiscard]] std::optional<Image> read_raw16(std::string_view bytes, int width, int height);
// The sizes a raw 16-bit file of `byte_count` bytes may be, squares first: none when it is no whole number of values.
[[nodiscard]] std::optional<std::pair<int, int>> guess_raw16_size(std::size_t byte_count);

// The bytes of `image` as a file of `format`, which must keep its depth: written_format converts it beforehand.
// PNG is written grey where the image is grey and without alpha where it is solid, and with no gamma or colour profile,
// so its values are the values painted. `quality` is JPEG's, from 1 to 100.
[[nodiscard]] std::string write_image(const Image& image, FileFormat format, int quality = 92);

} // namespace oui_paint

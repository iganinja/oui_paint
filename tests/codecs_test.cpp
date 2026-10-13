#include "codecs/codecs.h"

#include <gtest/gtest.h>

using namespace oui_paint;

namespace
{

Image read(const std::string& file)
{
    std::string error;
    const auto image{read_image(file, error)};
    EXPECT_TRUE(image.has_value()) << error;
    return image.value_or(Image{});
}

// The colour type of a PNG, from its header: 0 grey, 2 colour, 4 grey and alpha, 6 colour and alpha.
int png_color_type(const std::string& file)
{
    return static_cast<unsigned char>(file.at(25));
}

int png_bit_depth(const std::string& file)
{
    return static_cast<unsigned char>(file.at(24));
}

} // namespace

TEST(Codecs, EverySixteenBitGreyValueComesBackFromAPng)
{
    Image heights{PixelFormat::gray_alpha16, 256, 256};
    for(int y{}; y < 256; ++ y)
    {
        for(int x{}; x < 256; ++ x)
        {
            heights.set_pixel(x, y, GrayAlpha16{{static_cast<std::uint16_t>(y * 256 + x), GrayAlpha16::full}});
        }
    }
    const auto file{write_image(heights, FileFormat::png)};
    EXPECT_EQ(png_color_type(file), 0) << "solid grey is written as grey alone";
    EXPECT_EQ(png_bit_depth(file), 16);
    EXPECT_EQ(file.find("gAMA"), std::string::npos);
    EXPECT_EQ(file.find("sRGB"), std::string::npos);
    const auto back{read(file)};
    EXPECT_EQ(back.format, PixelFormat::gray_alpha16);
    EXPECT_EQ(back.bytes, heights.bytes);
}

TEST(Codecs, ColourWithAlphaComesBackFromAPngAtBothDepths)
{
    for(const auto format : {PixelFormat::rgba8, PixelFormat::rgba16, PixelFormat::gray_alpha8})
    {
        Image image{format, 3, 2};
        for(std::size_t index{}; index < image.bytes.size(); ++ index)
        {
            image.bytes[index] = static_cast<std::uint8_t>(index * 37u + 11u);
        }
        const auto file{write_image(image, FileFormat::png)};
        EXPECT_EQ(png_color_type(file), is_gray(format) ? 4 : 6);
        const auto back{read(file)};
        EXPECT_EQ(back.format, format);
        EXPECT_EQ(back.bytes, image.bytes) << describe(format);
    }
}

TEST(Codecs, ARawHeightmapComesBackAndItsSizeIsGuessedWhenSquare)
{
    Image heights{PixelFormat::gray_alpha16, 4, 4};
    for(int index{}; index < 16; ++ index)
    {
        heights.set_pixel(index % 4, index / 4, GrayAlpha16{{static_cast<std::uint16_t>(index * 4000 + 3), 65535}});
    }
    const auto file{write_image(heights, FileFormat::raw16)};
    ASSERT_EQ(file.size(), 32u);
    EXPECT_EQ(static_cast<unsigned char>(file[2]), (4003 & 0xff)) << "little-endian";
    EXPECT_EQ(guess_raw16_size(file.size()), std::make_pair(4, 4));
    EXPECT_FALSE(guess_raw16_size(24).has_value());
    const auto back{read_raw16(file, 4, 4)};
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->bytes, heights.bytes);
    EXPECT_FALSE(read_raw16(file, 4, 3).has_value());
}

TEST(Codecs, FormatsOfEightBitsTakeEightBitsAndSayWhatTheyKeep)
{
    EXPECT_TRUE(keeps_depth(FileFormat::png, PixelFormat::rgba16));
    EXPECT_FALSE(keeps_depth(FileFormat::jpeg, PixelFormat::rgba16));
    EXPECT_EQ(written_format(FileFormat::jpeg, PixelFormat::gray_alpha16), PixelFormat::gray_alpha8);
    EXPECT_EQ(written_format(FileFormat::raw16, PixelFormat::rgba8), PixelFormat::gray_alpha16);
    EXPECT_FALSE(can_write(FileFormat::gif));
    EXPECT_EQ(file_format_of("/a/b/Map.R16"), FileFormat::raw16);
    EXPECT_EQ(file_format_of("photo.JPEG"), FileFormat::jpeg);
    EXPECT_FALSE(file_format_of("notes.txt").has_value());
    EXPECT_THROW(static_cast<void>(write_image(Image{PixelFormat::rgba16, 1, 1}, FileFormat::jpeg)),
                 std::invalid_argument);
}

TEST(Codecs, BmpAndTgaComeBackAndJpegComesBackClose)
{
    Image image{PixelFormat::rgba8, 16, 16};
    for(int y{}; y < 16; ++ y)
    {
        for(int x{}; x < 16; ++ x)
        {
            image.set_pixel(x, y, Rgba8{{static_cast<std::uint8_t>(x * 16), static_cast<std::uint8_t>(y * 16), 90, 255}});
        }
    }
    EXPECT_EQ(read(write_image(image, FileFormat::bmp)).bytes, image.bytes);
    EXPECT_EQ(read(write_image(image, FileFormat::tga)).bytes, image.bytes);
    const auto jpeg{read(write_image(image, FileFormat::jpeg, 100))};
    ASSERT_EQ(jpeg.bytes.size(), image.bytes.size());
    for(std::size_t index{}; index < image.bytes.size(); ++ index)
    {
        EXPECT_NEAR(jpeg.bytes[index], image.bytes[index], 12);
    }
}

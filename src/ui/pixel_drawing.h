#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace oui_paint
{

// Pixels to draw a widget's picture in, as 0xRRGGBB, for Painter::draw_image: the curves' graph, the levels' bars.
struct PixelDrawing
{
    int width{0};
    int height{0};
    std::vector<std::uint32_t> pixels;

    void resize(int new_width, int new_height, std::uint32_t color)
    {
        width = std::max(0, new_width);
        height = std::max(0, new_height);
        pixels.assign(static_cast<std::size_t>(width) * height, color);
    }

    void put(int x, int y, std::uint32_t color)
    {
        if(x >= 0 && y >= 0 && x < width && y < height)
        {
            pixels[static_cast<std::size_t>(y) * width + x] = color;
        }
    }

    void fill(int x, int y, int fill_width, int fill_height, std::uint32_t color)
    {
        for(int row{std::max(0, y)}; row < std::min(height, y + fill_height); ++ row)
        {
            for(int column{std::max(0, x)}; column < std::min(width, x + fill_width); ++ column)
            {
                pixels[static_cast<std::size_t>(row) * width + column] = color;
            }
        }
    }

    // A line one pixel wide from one end to the other, both in.
    void line(int x0, int y0, int x1, int y1, std::uint32_t color)
    {
        const auto dx{std::abs(x1 - x0)};
        const auto dy{-std::abs(y1 - y0)};
        const auto step_x{x0 < x1 ? 1 : -1};
        const auto step_y{y0 < y1 ? 1 : -1};
        auto error{dx + dy};
        for(;;)
        {
            put(x0, y0, color);
            if(x0 == x1 && y0 == y1)
            {
                return;
            }
            const auto doubled{2 * error};
            if(doubled >= dy)
            {
                error += dy;
                x0 += step_x;
            }
            if(doubled <= dx)
            {
                error += dx;
                y0 += step_y;
            }
        }
    }

    // A triangle pointing up with its tip at x, y, `size` pixels tall, as a marker under a bar.
    void marker(int x, int y, int size, std::uint32_t fill_color, std::uint32_t edge)
    {
        for(int row{}; row < size; ++ row)
        {
            for(int column{-row}; column <= row; ++ column)
            {
                put(x + column, y + row, std::abs(column) == row || row == size - 1 ? edge : fill_color);
            }
        }
    }
};

} // namespace oui_paint

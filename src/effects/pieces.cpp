#include "effects/pieces.h"

#include "core/blend.h"
#include "tools/color_math.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>

namespace oui_paint
{

namespace
{

constexpr auto pi{3.14159265358979323846};

// Straight colour of a pixel, alpha apart.
void unpremultiply(const float* pixel, double& red, double& green, double& blue, double& alpha)
{
    alpha = pixel[3];
    const auto divide{alpha > 0.0 ? 1.0 / alpha : 0.0};
    red = pixel[0] * divide;
    green = pixel[1] * divide;
    blue = pixel[2] * divide;
}

void premultiply(float* pixel, double red, double green, double blue, double alpha)
{
    alpha = std::clamp(alpha, 0.0, 1.0);
    pixel[0] = static_cast<float>(std::clamp(red, 0.0, 1.0) * alpha);
    pixel[1] = static_cast<float>(std::clamp(green, 0.0, 1.0) * alpha);
    pixel[2] = static_cast<float>(std::clamp(blue, 0.0, 1.0) * alpha);
    pixel[3] = static_cast<float>(alpha);
}

// The average of the image at points along a path through each pixel, as motion, radial and zoom blurs take it.
template <typename Path> WorkImage along(const WorkImage& image, int samples, Path path)
{
    WorkImage result{image.width, image.height};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            double sum[4]{};
            for(int sample{}; sample < samples; ++ sample)
            {
                const auto t{samples == 1 ? 0.0 : static_cast<double>(sample) / (samples - 1) - 0.5};
                double sx{};
                double sy{};
                path(x + 0.5, y + 0.5, t, sx, sy);
                const auto* const pixel{image.clamped(static_cast<int>(std::floor(sx)), static_cast<int>(std::floor(sy)))};
                for(int channel{}; channel < 4; ++ channel)
                {
                    sum[channel] += pixel[channel];
                }
            }
            auto* const out{result.at(x, y)};
            for(int channel{}; channel < 4; ++ channel)
            {
                out[channel] = static_cast<float>(sum[channel] / samples);
            }
        }
    }
    return result;
}

// Perlin's gradient noise, from -1 to 1.
class Noise
{
  public:
    explicit Noise(std::uint32_t seed)
    {
        std::iota(m_permutation.begin(), m_permutation.begin() + 256, 0);
        auto state{seed == 0 ? 0x9e3779b9u : seed};
        for(int index{255}; index > 0; -- index)
        {
            state ^= state << 13u;
            state ^= state >> 17u;
            state ^= state << 5u;
            std::swap(m_permutation[static_cast<std::size_t>(index)],
                      m_permutation[state % static_cast<std::uint32_t>(index + 1)]);
        }
        std::copy(m_permutation.begin(), m_permutation.begin() + 256, m_permutation.begin() + 256);
    }

    [[nodiscard]] double at(double x, double y) const
    {
        const auto cell_x{static_cast<int>(std::floor(x)) & 255};
        const auto cell_y{static_cast<int>(std::floor(y)) & 255};
        const auto fx{x - std::floor(x)};
        const auto fy{y - std::floor(y)};
        const auto fade{[](double t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }};
        const auto u{fade(fx)};
        const auto v{fade(fy)};
        const auto corner{[&](int i, int j)
                          {
                              const auto hash{m_permutation[static_cast<std::size_t>(
                                  m_permutation[static_cast<std::size_t>(cell_x + i)] + cell_y + j)]};
                              const auto angle{hash * (2.0 * pi / 256.0)};
                              return std::cos(angle) * (fx - i) + std::sin(angle) * (fy - j);
                          }};
        const auto top{corner(0, 0) + u * (corner(1, 0) - corner(0, 0))};
        const auto bottom{corner(0, 1) + u * (corner(1, 1) - corner(0, 1))};
        return (top + v * (bottom - top)) * 1.4142135623730951;
    }

  private:
    std::array<int, 512> m_permutation{};
};

} // namespace

const float* WorkImage::clamped(int x, int y) const
{
    return at(std::clamp(x, 0, width - 1), std::clamp(y, 0, height - 1));
}

WorkImage to_work(const Image& image)
{
    WorkImage work{image.width, image.height};
    visit_format(image.format,
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     constexpr auto full{static_cast<double>(PixelType::full)};
                     for(int y{}; y < image.height; ++ y)
                     {
                         for(int x{}; x < image.width; ++ x)
                         {
                             const auto pixel{image.pixel<PixelType>(x, y)};
                             const auto alpha{pixel.alpha() / full};
                             if constexpr(PixelType::channels == 2)
                             {
                                 const auto gray{pixel.value[0] / full};
                                 premultiply(work.at(x, y), gray, gray, gray, alpha);
                             }
                             else
                             {
                                 premultiply(work.at(x, y), pixel.value[0] / full, pixel.value[1] / full,
                                             pixel.value[2] / full, alpha);
                             }
                         }
                     }
                 });
    return work;
}

Image from_work(const WorkImage& work, PixelFormat format)
{
    Image image{format, work.width, work.height};
    visit_format(format,
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     using Channel = typename PixelType::ChannelType;
                     constexpr auto full{static_cast<double>(PixelType::full)};
                     const auto channel{[](double value)
                                        { return static_cast<Channel>(std::lround(std::clamp(value, 0.0, 1.0) * full)); }};
                     for(int y{}; y < work.height; ++ y)
                     {
                         for(int x{}; x < work.width; ++ x)
                         {
                             double red{};
                             double green{};
                             double blue{};
                             double alpha{};
                             unpremultiply(work.at(x, y), red, green, blue, alpha);
                             PixelType pixel;
                             if constexpr(PixelType::channels == 2)
                             {
                                 pixel.value = {channel(0.299 * red + 0.587 * green + 0.114 * blue), channel(alpha)};
                             }
                             else
                             {
                                 pixel.value = {channel(red), channel(green), channel(blue), channel(alpha)};
                             }
                             image.set_pixel(x, y, pixel);
                         }
                     }
                 });
    return image;
}

WorkImage gaussian_blur(const WorkImage& image, double radius)
{
    if(radius <= 0.0)
    {
        return image;
    }
    const auto sigma{std::max(0.3, radius / 2.0)};
    const auto reach{static_cast<int>(std::ceil(radius))};
    std::vector<double> kernel(static_cast<std::size_t>(reach) * 2 + 1);
    auto total{0.0};
    for(int offset{-reach}; offset <= reach; ++ offset)
    {
        const auto weight{std::exp(-(offset * offset) / (2.0 * sigma * sigma))};
        kernel[static_cast<std::size_t>(offset + reach)] = weight;
        total += weight;
    }
    for(auto& weight : kernel)
    {
        weight /= total;
    }
    // Across, then down: a gaussian of two dimensions is two of one.
    const auto pass{[&](const WorkImage& source, bool horizontal)
                    {
                        WorkImage result{source.width, source.height};
                        for(int y{}; y < source.height; ++ y)
                        {
                            for(int x{}; x < source.width; ++ x)
                            {
                                double sum[4]{};
                                for(int offset{-reach}; offset <= reach; ++ offset)
                                {
                                    const auto* const pixel{horizontal ? source.clamped(x + offset, y)
                                                                       : source.clamped(x, y + offset)};
                                    const auto weight{kernel[static_cast<std::size_t>(offset + reach)]};
                                    for(int channel{}; channel < 4; ++ channel)
                                    {
                                        sum[channel] += pixel[channel] * weight;
                                    }
                                }
                                auto* const out{result.at(x, y)};
                                for(int channel{}; channel < 4; ++ channel)
                                {
                                    out[channel] = static_cast<float>(sum[channel]);
                                }
                            }
                        }
                        return result;
                    }};
    return pass(pass(image, true), false);
}

WorkImage box_blur(const WorkImage& image, int radius)
{
    std::vector<double> matrix(static_cast<std::size_t>(radius * 2 + 1) * static_cast<std::size_t>(radius * 2 + 1), 1.0);
    return convolve(image, matrix, radius * 2 + 1, static_cast<double>(matrix.size()), 0.0);
}

WorkImage motion_blur(const WorkImage& image, double degrees, double distance)
{
    const auto dx{std::cos(degrees * pi / 180.0) * distance};
    const auto dy{-std::sin(degrees * pi / 180.0) * distance};
    const auto samples{std::max(1, static_cast<int>(std::ceil(distance)) + 1)};
    return along(image, samples,
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     sx = x + dx * t;
                     sy = y + dy * t;
                 });
}

WorkImage radial_blur(const WorkImage& image, double degrees, double center_x, double center_y)
{
    const auto span{degrees * pi / 180.0};
    const auto farthest{std::hypot(std::max(center_x, image.width - center_x), std::max(center_y, image.height - center_y))};
    const auto samples{std::max(1, static_cast<int>(std::ceil(span * farthest / 2.0)) + 1)};
    return along(image, std::min(samples, 64),
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     const auto angle{span * t};
                     const auto rx{x - center_x};
                     const auto ry{y - center_y};
                     sx = center_x + rx * std::cos(angle) - ry * std::sin(angle);
                     sy = center_y + rx * std::sin(angle) + ry * std::cos(angle);
                 });
}

WorkImage zoom_blur(const WorkImage& image, double amount, double center_x, double center_y)
{
    const auto farthest{std::hypot(std::max(center_x, image.width - center_x), std::max(center_y, image.height - center_y))};
    const auto samples{std::max(1, std::min(64, static_cast<int>(std::ceil(amount * farthest / 2.0)) + 1))};
    return along(image, samples,
                 [&](double x, double y, double t, double& sx, double& sy)
                 {
                     // From the pixel towards the middle, half the amount each way.
                     const auto toward{(t + 0.5) * amount};
                     sx = x + (center_x - x) * toward;
                     sy = y + (center_y - y) * toward;
                 });
}

WorkImage convolve(const WorkImage& image, const std::vector<double>& matrix, int size, double divisor, double offset)
{
    WorkImage result{image.width, image.height};
    const auto half{size / 2};
    const auto divide{divisor != 0.0 ? 1.0 / divisor : 1.0};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            double sum[3]{};
            for(int j{}; j < size; ++ j)
            {
                for(int i{}; i < size; ++ i)
                {
                    const auto weight{matrix[static_cast<std::size_t>(j * size + i)]};
                    if(weight == 0.0)
                    {
                        continue;
                    }
                    double red{};
                    double green{};
                    double blue{};
                    double alpha{};
                    unpremultiply(image.clamped(x + i - half, y + j - half), red, green, blue, alpha);
                    sum[0] += red * weight;
                    sum[1] += green * weight;
                    sum[2] += blue * weight;
                }
            }
            premultiply(result.at(x, y), sum[0] * divide + offset, sum[1] * divide + offset, sum[2] * divide + offset,
                        image.at(x, y)[3]);
        }
    }
    return result;
}

WorkImage clouds(int width, int height, double scale, double roughness, std::uint32_t seed)
{
    WorkImage result{width, height};
    const Noise noise{seed};
    std::vector<double> values(static_cast<std::size_t>(width) * height);
    auto lowest{1e18};
    auto highest{-1e18};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            auto sum{0.0};
            auto amplitude{1.0};
            for(auto size{std::max(1.0, scale)}; size >= 1.0; size /= 2.0, amplitude *= roughness)
            {
                sum += noise.at(x / size + seed % 1000, y / size) * amplitude;
            }
            values[static_cast<std::size_t>(y) * width + x] = sum;
            lowest = std::min(lowest, sum);
            highest = std::max(highest, sum);
        }
    }
    const auto span{highest > lowest ? highest - lowest : 1.0};
    for(int y{}; y < height; ++ y)
    {
        for(int x{}; x < width; ++ x)
        {
            const auto gray{(values[static_cast<std::size_t>(y) * width + x] - lowest) / span};
            premultiply(result.at(x, y), gray, gray, gray, 1.0);
        }
    }
    return result;
}

WorkImage lookup(const WorkImage& image, const std::array<std::vector<double>, 3>& tables)
{
    const auto through{[](const std::vector<double>& table, double value)
                       {
                           if(table.empty())
                           {
                               return value;
                           }
                           const auto place{std::clamp(value, 0.0, 1.0) * (table.size() - 1)};
                           const auto low{static_cast<std::size_t>(std::floor(place))};
                           const auto high{std::min(low + 1, table.size() - 1)};
                           return table[low] + (table[high] - table[low]) * (place - low);
                       }};
    WorkImage result{image.width, image.height};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            double red{};
            double green{};
            double blue{};
            double alpha{};
            unpremultiply(image.at(x, y), red, green, blue, alpha);
            premultiply(result.at(x, y), through(tables[0], red), through(tables[1], green), through(tables[2], blue),
                        alpha);
        }
    }
    return result;
}

WorkImage lookup_luminosity(const WorkImage& image, const std::vector<double>& table)
{
    WorkImage result{image.width, image.height};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            double red{};
            double green{};
            double blue{};
            double alpha{};
            unpremultiply(image.at(x, y), red, green, blue, alpha);
            const auto luminosity{0.299 * red + 0.587 * green + 0.114 * blue};
            const auto place{std::clamp(luminosity, 0.0, 1.0) * (table.size() - 1)};
            const auto low{static_cast<std::size_t>(std::floor(place))};
            const auto high{std::min(low + 1, table.size() - 1)};
            const auto wanted{table[low] + (table[high] - table[low]) * (place - low)};
            if(luminosity <= 0.0)
            {
                premultiply(result.at(x, y), wanted, wanted, wanted, alpha);
                continue;
            }
            // The colour scaled to the luminosity wanted; what that pushes past white is spread towards white, so the
            // luminosity stays what the curve says.
            auto scaled_red{red * wanted / luminosity};
            auto scaled_green{green * wanted / luminosity};
            auto scaled_blue{blue * wanted / luminosity};
            const auto highest{std::max({scaled_red, scaled_green, scaled_blue})};
            if(highest > 1.0)
            {
                const auto towards{(highest - 1.0) / std::max(highest - wanted, 1e-9)};
                scaled_red += (wanted - scaled_red) * towards;
                scaled_green += (wanted - scaled_green) * towards;
                scaled_blue += (wanted - scaled_blue) * towards;
            }
            premultiply(result.at(x, y), std::clamp(scaled_red, 0.0, 1.0), std::clamp(scaled_green, 0.0, 1.0),
                        std::clamp(scaled_blue, 0.0, 1.0), alpha);
        }
    }
    return result;
}

WorkImage hue_saturation(const WorkImage& image, double hue, double saturation, double lightness)
{
    WorkImage result{image.width, image.height};
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            double red{};
            double green{};
            double blue{};
            double alpha{};
            unpremultiply(image.at(x, y), red, green, blue, alpha);
            auto hsv{to_hsv({red, green, blue, alpha})};
            hsv.hue += hue;
            hsv.saturation = std::clamp(hsv.saturation * (1.0 + saturation), 0.0, 1.0);
            auto color{from_hsv(hsv, alpha)};
            // Lightness towards white above 0, towards black below.
            const auto target{lightness > 0.0 ? 1.0 : 0.0};
            const auto amount{std::abs(lightness)};
            color.red += (target - color.red) * amount;
            color.green += (target - color.green) * amount;
            color.blue += (target - color.blue) * amount;
            premultiply(result.at(x, y), color.red, color.green, color.blue, alpha);
        }
    }
    return result;
}

WorkImage mix(const WorkImage& below, const WorkImage& above, BlendMode mode, double opacity)
{
    WorkImage result{below.width, below.height};
    for(int y{}; y < below.height; ++ y)
    {
        for(int x{}; x < below.width; ++ x)
        {
            double base[4]{};
            double over[4]{};
            unpremultiply(below.at(x, y), base[0], base[1], base[2], base[3]);
            if(x < above.width && y < above.height)
            {
                unpremultiply(above.at(x, y), over[0], over[1], over[2], over[3]);
            }
            Rgba16 bottom;
            Rgba16 top;
            for(int channel{}; channel < 4; ++ channel)
            {
                bottom.value[static_cast<std::size_t>(channel)] =
                    static_cast<std::uint16_t>(std::lround(std::clamp(base[channel], 0.0, 1.0) * 65535.0));
                top.value[static_cast<std::size_t>(channel)] =
                    static_cast<std::uint16_t>(std::lround(std::clamp(over[channel], 0.0, 1.0) * 65535.0));
            }
            const auto mixed{blend(bottom, top, opacity, mode)};
            premultiply(result.at(x, y), mixed.value[0] / 65535.0, mixed.value[1] / 65535.0, mixed.value[2] / 65535.0,
                        mixed.value[3] / 65535.0);
        }
    }
    return result;
}

} // namespace oui_paint

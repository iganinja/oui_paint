#include "effects/indexing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>

namespace oui_paint
{

namespace
{

// A pixel's colour as it shows, without the alpha the work image multiplies it by, and whether it counts as opaque.
struct Shown
{
    double red;
    double green;
    double blue;
    bool opaque;
};

[[nodiscard]] Shown shown(const float* pixel)
{
    const auto alpha{static_cast<double>(pixel[3])};
    if(alpha < 0.5)
    {
        return {0.0, 0.0, 0.0, false};
    }
    return {pixel[0] / alpha, pixel[1] / alpha, pixel[2] / alpha, true};
}

[[nodiscard]] std::uint32_t key_of(double red, double green, double blue)
{
    const auto byte{[](double value)
                    { return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0, 1.0) * 255.0)); }};
    return byte(red) << 16u | byte(green) << 8u | byte(blue);
}

[[nodiscard]] Color color_of(std::uint32_t key)
{
    return {(key >> 16u & 0xffu) / 255.0, (key >> 8u & 0xffu) / 255.0, (key & 0xffu) / 255.0, 1.0};
}

// The palette's index nearest a colour, by the distance between them as numbers from 0 to 1.
[[nodiscard]] std::size_t nearest(const std::vector<Color>& palette, double red, double green, double blue)
{
    std::size_t best{};
    auto shortest{std::numeric_limits<double>::max()};
    for(std::size_t index{}; index < palette.size(); ++ index)
    {
        const auto dr{palette[index].red - red};
        const auto dg{palette[index].green - green};
        const auto db{palette[index].blue - blue};
        const auto distance{dr * dr + dg * dg + db * db};
        if(distance < shortest)
        {
            shortest = distance;
            best = index;
        }
    }
    return best;
}

void put_index(float* pixel, std::size_t index, bool opaque)
{
    pixel[0] = opaque ? static_cast<float>(index) / 255.0f : 0.0f;
    pixel[1] = 0.0f;
    pixel[2] = 0.0f;
    pixel[3] = opaque ? 1.0f : 0.0f;
}

// The Bayer matrix of `size` 2, 4 or 8, row by row, its values from 0 to size² - 1, built from the one of half its
// size.
[[nodiscard]] std::vector<int> bayer_matrix(int size)
{
    std::vector<int> matrix{0, 2, 3, 1};
    for(int side{2}; side < size; side *= 2)
    {
        std::vector<int> bigger(static_cast<std::size_t>(side * 2 * side * 2));
        for(int y{}; y < side * 2; ++ y)
        {
            for(int x{}; x < side * 2; ++ x)
            {
                const auto base{matrix[static_cast<std::size_t>((y % side) * side + x % side)] * 4};
                const auto quarter{(y / side) * 2 + x / side};
                static constexpr int order[]{0, 2, 3, 1};
                bigger[static_cast<std::size_t>(y * side * 2 + x)] = base + order[quarter];
            }
        }
        matrix = std::move(bigger);
    }
    return matrix;
}

} // namespace

std::vector<std::pair<Color, std::size_t>> distinct_colors(const WorkImage& image, std::size_t most)
{
    std::map<std::uint32_t, std::size_t> counts;
    for(int y{}; y < image.height; ++ y)
    {
        for(int x{}; x < image.width; ++ x)
        {
            const auto pixel{shown(image.at(x, y))};
            if(pixel.opaque)
            {
                ++ counts[key_of(pixel.red, pixel.green, pixel.blue)];
            }
        }
    }
    std::vector<std::pair<std::uint32_t, std::size_t>> sorted{counts.begin(), counts.end()};
    // The commonest first, and among as common the lower key first, so the same image gives the same order.
    std::stable_sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    if(sorted.size() > most)
    {
        sorted.resize(most);
    }
    std::vector<std::pair<Color, std::size_t>> found;
    for(const auto& [key, count] : sorted)
    {
        found.emplace_back(color_of(key), count);
    }
    return found;
}

std::vector<Color> median_cut(const WorkImage& image, int count)
{
    // Every colour there is, with how many pixels have it.
    std::vector<std::pair<std::uint32_t, std::size_t>> colors;
    {
        std::map<std::uint32_t, std::size_t> counts;
        for(int y{}; y < image.height; ++ y)
        {
            for(int x{}; x < image.width; ++ x)
            {
                const auto pixel{shown(image.at(x, y))};
                if(pixel.opaque)
                {
                    ++ counts[key_of(pixel.red, pixel.green, pixel.blue)];
                }
            }
        }
        colors.assign(counts.begin(), counts.end());
    }
    if(colors.empty() || count <= 0)
    {
        return {};
    }
    const auto channel{[](std::uint32_t key, int which) { return static_cast<int>(key >> (16 - 8 * which) & 0xffu); }};
    // Boxes of colours, as ranges of `colors`, split until there are enough or none can be.
    struct Box
    {
        std::size_t begin;
        std::size_t end;
    };
    std::vector<Box> boxes{{0, colors.size()}};
    const auto spread{[&](const Box& box, int& widest)
                      {
                          auto best{-1};
                          for(int which{}; which < 3; ++ which)
                          {
                              auto low{255};
                              auto high{0};
                              for(auto index{box.begin}; index < box.end; ++ index)
                              {
                                  low = std::min(low, channel(colors[index].first, which));
                                  high = std::max(high, channel(colors[index].first, which));
                              }
                              if(high - low > best)
                              {
                                  best = high - low;
                                  widest = which;
                              }
                          }
                          return best;
                      }};
    while(static_cast<int>(boxes.size()) < count)
    {
        // The box with the widest spread, of those with two colours at least.
        auto chosen{boxes.size()};
        auto chosen_spread{0};
        auto chosen_channel{0};
        for(std::size_t index{}; index < boxes.size(); ++ index)
        {
            if(boxes[index].end - boxes[index].begin < 2)
            {
                continue;
            }
            auto widest{0};
            const auto range{spread(boxes[index], widest)};
            if(range > chosen_spread)
            {
                chosen_spread = range;
                chosen = index;
                chosen_channel = widest;
            }
        }
        if(chosen == boxes.size())
        {
            break;
        }
        auto& box{boxes[chosen]};
        std::sort(colors.begin() + static_cast<std::ptrdiff_t>(box.begin),
                  colors.begin() + static_cast<std::ptrdiff_t>(box.end),
                  [&](const auto& a, const auto& b)
                  { return channel(a.first, chosen_channel) < channel(b.first, chosen_channel); });
        // Split where half the pixels are on either side, never leaving one side empty.
        std::size_t total{};
        for(auto index{box.begin}; index < box.end; ++ index)
        {
            total += colors[index].second;
        }
        std::size_t seen{};
        auto middle{box.begin + 1};
        for(auto index{box.begin}; index < box.end - 1; ++ index)
        {
            seen += colors[index].second;
            middle = index + 1;
            if(seen * 2 >= total)
            {
                break;
            }
        }
        const Box upper{middle, box.end};
        box.end = middle;
        boxes.push_back(upper);
    }
    std::vector<Color> palette;
    for(const auto& box : boxes)
    {
        double red{};
        double green{};
        double blue{};
        double weight{};
        for(auto index{box.begin}; index < box.end; ++ index)
        {
            const auto color{color_of(colors[index].first)};
            const auto pixels{static_cast<double>(colors[index].second)};
            red += color.red * pixels;
            green += color.green * pixels;
            blue += color.blue * pixels;
            weight += pixels;
        }
        palette.push_back(color_of(key_of(red / weight, green / weight, blue / weight)));
    }
    return palette;
}

WorkImage nearest_indices(const WorkImage& image, const std::vector<Color>& palette)
{
    WorkImage indices{image.width, image.height};
    for_each_row(image.height,
                 [&](int y)
                 {
                     // Neighbours are often the same colour: the last one found is tried first.
                     std::uint32_t last_key{0xffffffffu};
                     std::size_t last_index{};
                     for(int x{}; x < image.width; ++ x)
                     {
                         const auto pixel{shown(image.at(x, y))};
                         if(pixel.opaque && !palette.empty())
                         {
                             const auto key{key_of(pixel.red, pixel.green, pixel.blue)};
                             if(key != last_key)
                             {
                                 last_key = key;
                                 last_index = nearest(palette, pixel.red, pixel.green, pixel.blue);
                             }
                             put_index(indices.at(x, y), last_index, true);
                         }
                         else
                         {
                             put_index(indices.at(x, y), 0, false);
                         }
                     }
                 });
    return indices;
}

WorkImage floyd_steinberg_indices(const WorkImage& image, const std::vector<Color>& palette, double strength)
{
    WorkImage indices{image.width, image.height};
    // What each pixel missed, carried to the row it is in and the next, three numbers a pixel.
    std::vector<double> current(static_cast<std::size_t>(image.width + 2) * 3);
    std::vector<double> next(current.size());
    for(int y{}; y < image.height; ++ y)
    {
        std::fill(next.begin(), next.end(), 0.0);
        // Back and forth, so the error does not drift one way.
        const auto forward{y % 2 == 0};
        for(int step{}; step < image.width; ++ step)
        {
            const auto x{forward ? step : image.width - 1 - step};
            const auto pixel{shown(image.at(x, y))};
            if(!pixel.opaque || palette.empty())
            {
                put_index(indices.at(x, y), 0, false);
                continue;
            }
            const auto at{static_cast<std::size_t>(x + 1) * 3};
            const auto red{std::clamp(pixel.red + current[at], 0.0, 1.0)};
            const auto green{std::clamp(pixel.green + current[at + 1], 0.0, 1.0)};
            const auto blue{std::clamp(pixel.blue + current[at + 2], 0.0, 1.0)};
            const auto index{nearest(palette, red, green, blue)};
            put_index(indices.at(x, y), index, true);
            const std::array<double, 3> missed{(red - palette[index].red) * strength,
                                               (green - palette[index].green) * strength,
                                               (blue - palette[index].blue) * strength};
            const auto ahead{forward ? 1 : -1};
            for(int which{}; which < 3; ++ which)
            {
                const auto error{missed[static_cast<std::size_t>(which)]};
                current[static_cast<std::size_t>(x + 1 + ahead) * 3 + which] += error * 7.0 / 16.0;
                next[static_cast<std::size_t>(x + 1 - ahead) * 3 + which] += error * 3.0 / 16.0;
                next[static_cast<std::size_t>(x + 1) * 3 + which] += error * 5.0 / 16.0;
                next[static_cast<std::size_t>(x + 1 + ahead) * 3 + which] += error * 1.0 / 16.0;
            }
        }
        std::swap(current, next);
    }
    return indices;
}

WorkImage bayer_indices(const WorkImage& image, const std::vector<Color>& palette, int size, double strength)
{
    size = size <= 2 ? 2 : size <= 4 ? 4 : 8;
    const auto matrix{bayer_matrix(size)};
    // As far as a pixel may be nudged: about the step between the palette's colours, as if spread evenly.
    const auto reach{palette.empty() ? 0.0 : strength / std::cbrt(static_cast<double>(palette.size()))};
    WorkImage indices{image.width, image.height};
    for_each_row(image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         const auto pixel{shown(image.at(x, y))};
                         if(!pixel.opaque || palette.empty())
                         {
                             put_index(indices.at(x, y), 0, false);
                             continue;
                         }
                         const auto threshold{
                             (matrix[static_cast<std::size_t>((y % size) * size + x % size)] + 0.5) / (size * size) -
                             0.5};
                         const auto nudge{threshold * reach};
                         put_index(indices.at(x, y),
                                   nearest(palette, pixel.red + nudge, pixel.green + nudge, pixel.blue + nudge), true);
                     }
                 });
    return indices;
}

WorkImage grayscale(const WorkImage& image, double red, double green, double blue)
{
    WorkImage gray{image.width, image.height};
    for_each_row(image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         // Colour times alpha, so the sum is too: it stays as it was multiplied.
                         const auto* const pixel{image.at(x, y)};
                         const auto value{
                             std::clamp(static_cast<float>(pixel[0] * red + pixel[1] * green + pixel[2] * blue), 0.0f,
                                        pixel[3])};
                         auto* const out{gray.at(x, y)};
                         out[0] = out[1] = out[2] = value;
                         out[3] = pixel[3];
                     }
                 });
    return gray;
}

WorkImage dither_levels(const WorkImage& image, int levels, int size, double strength)
{
    size = size <= 2 ? 2 : size <= 4 ? 4 : 8;
    const auto matrix{bayer_matrix(size)};
    const auto steps{static_cast<double>(std::max(2, levels) - 1)};
    WorkImage result{image.width, image.height};
    for_each_row(image.height,
                 [&](int y)
                 {
                     for(int x{}; x < image.width; ++ x)
                     {
                         const auto* const pixel{image.at(x, y)};
                         auto* const out{result.at(x, y)};
                         const auto alpha{static_cast<double>(pixel[3])};
                         const auto nudge{
                             ((matrix[static_cast<std::size_t>((y % size) * size + x % size)] + 0.5) / (size * size) -
                              0.5) *
                             strength};
                         const auto level{[&](double value) {
                             return std::clamp(std::floor(value * steps + 0.5 + nudge), 0.0, steps) / steps;
                         }};
                         const auto new_alpha{level(alpha)};
                         for(int channel{}; channel < 3; ++ channel)
                         {
                             // The colour as it shows, brought to its level, multiplied again by the new alpha.
                             const auto shown{alpha > 0.0 ? pixel[channel] / alpha : 0.0};
                             out[channel] = static_cast<float>(level(shown) * new_alpha);
                         }
                         out[3] = static_cast<float>(new_alpha);
                     }
                 });
    return result;
}

} // namespace oui_paint

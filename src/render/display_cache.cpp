#include "render/display_cache.h"

#include "core/blend.h"

#include <algorithm>
#include <cmath>

namespace oui_paint
{

namespace
{

template <typename PixelType> [[nodiscard]] std::uint32_t to_screen(const PixelType& pixel)
{
    const auto byte{[](auto channel)
                    {
                        if constexpr(sizeof(channel) == 1)
                        {
                            return static_cast<std::uint32_t>(channel);
                        }
                        else
                        {
                            return static_cast<std::uint32_t>((static_cast<unsigned>(channel) * 255u + 32767u) /
                                                              65535u);
                        }
                    }};
    if constexpr(PixelType::channels == 2)
    {
        const auto gray{byte(pixel.value[0])};
        return byte(pixel.value[1]) << 24u | gray << 16u | gray << 8u | gray;
    }
    else
    {
        return byte(pixel.value[3]) << 24u | byte(pixel.value[0]) << 16u | byte(pixel.value[1]) << 8u |
               byte(pixel.value[2]);
    }
}

// Four pixels averaged, each colour counted by its alpha.
[[nodiscard]] std::uint32_t average(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d)
{
    std::uint32_t alpha{};
    std::uint32_t red{};
    std::uint32_t green{};
    std::uint32_t blue{};
    for(const auto pixel : {a, b, c, d})
    {
        const auto weight{pixel >> 24u};
        alpha += weight;
        red += (pixel >> 16u & 0xffu) * weight;
        green += (pixel >> 8u & 0xffu) * weight;
        blue += (pixel & 0xffu) * weight;
    }
    if(alpha == 0)
    {
        return 0;
    }
    return (alpha + 2u) / 4u << 24u | (red + alpha / 2u) / alpha << 16u | (green + alpha / 2u) / alpha << 8u |
           (blue + alpha / 2u) / alpha;
}

} // namespace

DisplayCache::DisplayCache(const Document& document) : m_document{document}
{
}

void DisplayCache::invalidate_all()
{
    m_tiles.clear();
}

void DisplayCache::invalidate(TilePoint point)
{
    for(int level{}; level < levels; ++ level)
    {
        m_tiles.erase({level, point});
    }
}

const std::vector<std::uint32_t>& DisplayCache::tile(TilePoint point, int level)
{
    level = std::clamp(level, 0, levels - 1);
    const auto key{std::make_pair(level, point)};
    if(const auto found{m_tiles.find(key)}; found != m_tiles.end())
    {
        return found->second;
    }
    auto made{level == 0 ? mixed(point) : halved(point, level)};
    return m_tiles.emplace(key, std::move(made)).first->second;
}

std::vector<std::uint32_t> DisplayCache::mixed(TilePoint point) const
{
    std::vector<std::uint32_t> result(static_cast<std::size_t>(tile_size) * tile_size);
    visit_format(m_document.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     std::vector<PixelType> sum(result.size());
                     for(const auto& layer : m_document.layers())
                     {
                         const auto source{layer.tile(point)};
                         if(!layer.visible() || layer.opacity() <= 0.0 || source == nullptr)
                         {
                             continue;
                         }
                         for(int y{}; y < tile_size; ++ y)
                         {
                             for(int x{}; x < tile_size; ++ x)
                             {
                                 const auto above{source->template pixel<PixelType>(x, y)};
                                 if(above.alpha() == 0)
                                 {
                                     continue;
                                 }
                                 auto& below{sum[static_cast<std::size_t>(y * tile_size + x)]};
                                 below = blend(below, above, layer.opacity(), layer.blend_mode());
                             }
                         }
                     }
                     std::transform(sum.begin(), sum.end(), result.begin(),
                                    [](const PixelType& pixel) { return to_screen(pixel); });
                 });
    return result;
}

std::vector<std::uint32_t> DisplayCache::halved(TilePoint point, int level)
{
    const auto& above{tile(point, level - 1)};
    const auto side{tile_size >> level};
    const auto above_side{side * 2};
    std::vector<std::uint32_t> result(static_cast<std::size_t>(side) * side);
    for(int y{}; y < side; ++ y)
    {
        for(int x{}; x < side; ++ x)
        {
            const auto at{static_cast<std::size_t>(y * 2 * above_side + x * 2)};
            result[static_cast<std::size_t>(y * side + x)] =
                average(above[at], above[at + 1], above[at + above_side], above[at + above_side + 1]);
        }
    }
    return result;
}

void render_view(const Viewport& viewport, DisplayCache& cache, int image_width, int image_height,
                 std::uint32_t* pixels, int width, int height, std::uint32_t background)
{
    const auto zoom{viewport.zoom()};
    const auto level{zoom >= 1.0 ? 0
                                 : std::clamp(static_cast<int>(std::floor(std::log2(1.0 / zoom) + 1e-9)), 0,
                                              DisplayCache::levels - 1)};
    const auto scale{1 << level};
    const auto side{tile_size >> level};
    const auto origin{viewport.origin()};
    constexpr std::uint32_t light{0xffffffu};
    constexpr std::uint32_t dark{0xccccccu};
    constexpr int check{8};
    for(int y{}; y < height; ++ y)
    {
        auto* const row{pixels + static_cast<std::ptrdiff_t>(y) * width};
        const auto image_y{static_cast<int>(std::floor((y + 0.5 - origin.y) / zoom))};
        if(image_y < 0 || image_y >= image_height)
        {
            std::fill(row, row + width, background);
            continue;
        }
        const auto level_y{image_y / scale};
        const TilePoint tile_row{0, level_y / side};
        const auto in_tile_y{level_y % side};
        const std::vector<std::uint32_t>* tile{};
        auto tile_x{-1};
        for(int x{}; x < width; ++ x)
        {
            const auto image_x{static_cast<int>(std::floor((x + 0.5 - origin.x) / zoom))};
            if(image_x < 0 || image_x >= image_width)
            {
                row[x] = background;
                continue;
            }
            const auto level_x{image_x / scale};
            if(level_x / side != tile_x)
            {
                tile_x = level_x / side;
                tile = &cache.tile({tile_x, tile_row.y}, level);
            }
            const auto pixel{(*tile)[static_cast<std::size_t>(in_tile_y * side + level_x % side)]};
            const auto alpha{pixel >> 24u};
            const auto under{((x / check + y / check) % 2 == 0) ? light : dark};
            if(alpha == 255u)
            {
                row[x] = pixel & 0xffffffu;
                continue;
            }
            const auto mix{[alpha](std::uint32_t top, std::uint32_t bottom)
                           { return (top * alpha + bottom * (255u - alpha) + 127u) / 255u; }};
            row[x] = mix(pixel >> 16u & 0xffu, under >> 16u & 0xffu) << 16u |
                     mix(pixel >> 8u & 0xffu, under >> 8u & 0xffu) << 8u | mix(pixel & 0xffu, under & 0xffu);
        }
    }
}

void draw_selection_edge(const Viewport& viewport, const Selection& selection, std::uint32_t* pixels, int width,
                         int height, int phase)
{
    if(!selection.active())
    {
        return;
    }
    const auto zoom{viewport.zoom()};
    const auto origin{viewport.origin()};
    // Whether the image pixel under each view pixel is selected, a row at a time, and the row under it.
    const auto image_x{[&](int x) { return static_cast<int>(std::floor((x + 0.5 - origin.x) / zoom)); }};
    const auto image_y{[&](int y) { return static_cast<int>(std::floor((y + 0.5 - origin.y) / zoom)); }};
    std::vector<int> columns(static_cast<std::size_t>(width) + 1);
    for(int x{}; x <= width; ++ x)
    {
        columns[static_cast<std::size_t>(x)] = image_x(x);
    }
    const auto inside_row{[&](int y, std::vector<bool>& row)
                          {
                              const auto at_y{image_y(y)};
                              for(int x{}; x <= width; ++ x)
                              {
                                  row[static_cast<std::size_t>(x)] =
                                      selection.at(columns[static_cast<std::size_t>(x)], at_y) >= 128;
                              }
                          }};
    std::vector<bool> current(static_cast<std::size_t>(width) + 1);
    std::vector<bool> below(static_cast<std::size_t>(width) + 1);
    inside_row(0, current);
    for(int y{}; y < height; ++ y)
    {
        inside_row(y + 1, below);
        for(int x{}; x < width; ++ x)
        {
            const auto here{current[static_cast<std::size_t>(x)]};
            if(here == current[static_cast<std::size_t>(x) + 1] && here == below[static_cast<std::size_t>(x)])
            {
                continue;
            }
            pixels[static_cast<std::ptrdiff_t>(y) * width + x] = ((x + y + phase) / 4) % 2 == 0 ? 0x000000u : 0xffffffu;
        }
        std::swap(current, below);
    }
}

} // namespace oui_paint

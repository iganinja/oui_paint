#include "tools/text.h"

#include "core/blend.h"

#include <oui/utf8.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace oui_paint
{

namespace
{

// Each glyph of `text` with the top left of its pixels and the baseline of its line, as draw_text lays them out.
template <typename Visit>
void each_glyph(olagarro::oui::Font& font, std::string_view text, int left, int top, Visit&& visit)
{
    auto pen{left};
    std::size_t index{};
    while(index < text.size())
    {
        const auto character{olagarro::oui::decode_utf8(text, index)};
        if(character == U'\n')
        {
            pen = left;
            top += font.line_height();
            continue;
        }
        const auto& glyph{font.glyph(character)};
        const auto baseline{top + font.ascent()};
        visit(glyph, pen + glyph.left, baseline + glyph.top, baseline);
        pen += glyph.advance;
    }
}

// A line of text: where it starts and ends, as far as its pen went, and its baseline.
struct TextLine
{
    int left;
    int right;
    int baseline;
};

[[nodiscard]] std::vector<TextLine> lines_of(olagarro::oui::Font& font, std::string_view text, int left, int top)
{
    std::vector<TextLine> lines{{left, left, top + font.ascent()}};
    std::size_t index{};
    while(index < text.size())
    {
        const auto character{olagarro::oui::decode_utf8(text, index)};
        if(character == U'\n')
        {
            lines.push_back({left, left, lines.back().baseline + font.line_height()});
            continue;
        }
        lines.back().right += font.glyph(character).advance;
    }
    return lines;
}

// How far a letter leans for every pixel above its baseline when it is slanted, as an italic of about 11 degrees.
constexpr double lean{0.2};

// The squared distance along a line to the nearest point where `line` is 0, where `line` holds 0 at the points and a
// huge number elsewhere, or the squared distances of a pass already made: the lower envelope of parabolas, as
// Felzenszwalb and Huttenlocher's distance transform makes it.
void squared_distances(std::vector<float>& line)
{
    const auto count{line.size()};
    const auto height{[&line](std::size_t at) { return static_cast<double>(line[at]) + static_cast<double>(at * at); }};
    std::vector<std::size_t> roots(count);
    std::vector<double> bounds(count + 1);
    std::size_t hull{};
    bounds[0] = -std::numeric_limits<double>::infinity();
    bounds[1] = std::numeric_limits<double>::infinity();
    for(std::size_t at{1}; at < count; ++ at)
    {
        auto crossing{(height(at) - height(roots[hull])) / (2.0 * static_cast<double>(at - roots[hull]))};
        while(crossing <= bounds[hull])
        {
            -- hull;
            crossing = (height(at) - height(roots[hull])) / (2.0 * static_cast<double>(at - roots[hull]));
        }
        ++ hull;
        roots[hull] = at;
        bounds[hull] = crossing;
        bounds[hull + 1] = std::numeric_limits<double>::infinity();
    }
    std::vector<float> result(count);
    hull = 0;
    for(std::size_t at{}; at < count; ++ at)
    {
        while(bounds[hull + 1] < static_cast<double>(at))
        {
            ++ hull;
        }
        const auto offset{static_cast<double>(at) - static_cast<double>(roots[hull])};
        result[at] = static_cast<float>(offset * offset + static_cast<double>(line[roots[hull]]));
    }
    line = std::move(result);
}

// How far each pixel of a `width` by `height` box is from the nearest pixel of `inside`, 0 on them.
std::vector<float> distances(const std::vector<bool>& inside, int width, int height)
{
    const auto far{1e20f};
    std::vector<float> squared(inside.size());
    for(std::size_t at{}; at < inside.size(); ++ at)
    {
        squared[at] = inside[at] ? 0.0f : far;
    }
    std::vector<float> line(static_cast<std::size_t>(height));
    for(int x{}; x < width; ++ x)
    {
        for(int y{}; y < height; ++ y)
        {
            line[static_cast<std::size_t>(y)] = squared[static_cast<std::size_t>(y) * width + x];
        }
        squared_distances(line);
        for(int y{}; y < height; ++ y)
        {
            squared[static_cast<std::size_t>(y) * width + x] = line[static_cast<std::size_t>(y)];
        }
    }
    line.resize(static_cast<std::size_t>(width));
    for(int y{}; y < height; ++ y)
    {
        std::copy_n(squared.begin() + static_cast<std::ptrdiff_t>(y) * width, width, line.begin());
        squared_distances(line);
        std::copy(line.begin(), line.end(), squared.begin() + static_cast<std::ptrdiff_t>(y) * width);
    }
    for(auto& value : squared)
    {
        value = std::sqrt(value);
    }
    return squared;
}

} // namespace

std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias,
                                 const TextEffects& effects, const TextFace& face)
{
    const auto left{static_cast<int>(std::lround(at.x))};
    const auto top{static_cast<int>(std::lround(at.y))};

    // How much thicker a letter is made, and how thick a line through the text is: in proportion to the font.
    const auto thicker{face.embolden ? std::max(1, static_cast<int>(std::lround(font.line_height() / 18.0))) : 0};
    const auto stroke{std::max(1, static_cast<int>(std::lround(font.line_height() / 16.0)))};
    const auto lines{lines_of(font, text, left, top)};
    // Where the line through the text goes in a line of it: about halfway up its small letters.
    const auto strike_top{[&](const TextLine& line)
                          { return line.baseline - static_cast<int>(std::lround(font.ascent() * 0.3)) - stroke / 2; }};

    // The pixels the glyphs cover, with room around them for what goes around the text.
    auto first_x{std::numeric_limits<int>::max()};
    auto first_y{std::numeric_limits<int>::max()};
    auto last_x{std::numeric_limits<int>::min()};
    auto last_y{std::numeric_limits<int>::min()};
    const auto cover{[&](int x0, int y0, int x1, int y1)
                     {
                         first_x = std::min(first_x, x0);
                         first_y = std::min(first_y, y0);
                         last_x = std::max(last_x, x1);
                         last_y = std::max(last_y, y1);
                     }};
    each_glyph(font, text, left, top,
               [&](const auto& glyph, int x, int y, int baseline)
               {
                   if(glyph.width == 0 || glyph.height == 0)
                   {
                       return;
                   }
                   // Slanted, its bottom row moves the least, or left below the baseline, and its top row the most.
                   const auto bottom{y + glyph.height - 1};
                   const auto lowest{face.slant ? static_cast<int>(std::floor(lean * (baseline - bottom - 0.5))) : 0};
                   const auto highest{face.slant ? static_cast<int>(std::floor(lean * (baseline - y - 0.5))) + 1 : 0};
                   cover(x + lowest, y, x + glyph.width - 1 + highest, y + glyph.height - 1);
               });
    if(face.strikethrough)
    {
        for(const auto& line : lines)
        {
            if(line.right > line.left)
            {
                cover(line.left, strike_top(line), line.right - 1, strike_top(line) + stroke - 1);
            }
        }
    }
    if(first_x > last_x)
    {
        return {};
    }
    // A thicker letter grows to the right.
    last_x += thicker;
    const auto outline{std::max(effects.outline, 0)};
    const auto shadow{std::max(effects.shadow, 0)};
    const auto glow{std::max(effects.glow, 0)};
    const auto softness{std::max(1.0, shadow / 2.0)};
    const auto room{effects.any() ? std::max({outline + 2, outline + glow + 2,
                                              outline + shadow + static_cast<int>(std::ceil(softness)) + 2})
                                  : 0};
    const auto box_x{first_x - room};
    const auto box_y{first_y - room};
    const auto width{last_x - first_x + 1 + 2 * room};
    const auto height{last_y - first_y + 1 + 2 * room};
    const auto cell{[width](int x, int y) { return static_cast<std::size_t>(y) * width + x; }};

    std::vector<float> coverage(static_cast<std::size_t>(width) * height);
    const auto add{[&](int x, int y, float amount)
                   {
                       auto& value{coverage[cell(x - box_x, y - box_y)]};
                       value = std::min(1.0f, std::max(value, amount));
                   }};
    each_glyph(font, text, left, top,
               [&](const auto& glyph, int x, int y, int baseline)
               {
                   for(int row{}; row < glyph.height; ++ row)
                   {
                       // A slanted row moves right by how far it is above the baseline, shared between the two
                       // columns it falls between.
                       // Without antialiasing the row moves whole, to the nearest column.
                       const auto shift{face.slant ? lean * (baseline - (y + row) - 0.5) : 0.0};
                       const auto whole{static_cast<int>(antialias ? std::floor(shift) : std::round(shift))};
                       const auto part{antialias ? static_cast<float>(shift - whole) : 0.0f};
                       for(int column{}; column < glyph.width; ++ column)
                       {
                           const auto value{glyph.coverage[static_cast<std::size_t>(row) * glyph.width + column] /
                                            255.0f};
                           if(value <= 0.0f)
                           {
                               continue;
                           }
                           const auto image_x{x + column + whole};
                           if(face.slant)
                           {
                               auto& here{coverage[cell(image_x - box_x, y + row - box_y)]};
                               here = std::min(1.0f, here + value * (1.0f - part));
                               auto& next{coverage[cell(image_x + 1 - box_x, y + row - box_y)]};
                               next = std::min(1.0f, next + value * part);
                           }
                           else
                           {
                               add(image_x, y + row, value);
                           }
                       }
                   }
               });
    // Thicker: each pixel as covered as the most covered of those up to `thicker` to its left.
    if(thicker > 0)
    {
        const auto before{coverage};
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < width; ++ x)
            {
                auto& value{coverage[cell(x, y)]};
                for(int step{1}; step <= thicker && x - step >= 0; ++ step)
                {
                    value = std::max(value, before[cell(x - step, y)]);
                }
            }
        }
    }
    if(face.strikethrough)
    {
        for(const auto& line : lines)
        {
            for(auto y{strike_top(line)}; y < strike_top(line) + stroke; ++ y)
            {
                for(auto x{line.left}; x < line.right; ++ x)
                {
                    add(x, y, 1.0f);
                }
            }
        }
    }

    std::set<TilePoint> touched;
    visit_format(layer.format(),
                 [&](auto tag)
                 {
                     using PixelType = typename decltype(tag)::Type;
                     using Channel = typename PixelType::ChannelType;
                     const auto channel{[](double value)
                                        {
                                            return static_cast<Channel>(
                                                std::lround(std::clamp(value, 0.0, 1.0) * PixelType::full));
                                        }};
                     const auto pixel_of{[&](const Color& ink)
                                         {
                                             PixelType made;
                                             if constexpr(PixelType::channels == 2)
                                             {
                                                 made.value = {
                                                     channel(0.299 * ink.red + 0.587 * ink.green + 0.114 * ink.blue),
                                                     channel(ink.alpha)};
                                             }
                                             else
                                             {
                                                 made.value = {channel(ink.red), channel(ink.green), channel(ink.blue),
                                                               channel(ink.alpha)};
                                             }
                                             return made;
                                         }};
                     // Mixes `ink` over the layer by how much `amount` gives each pixel of the box.
                     const auto mix{[&](const Color& ink, const auto& amount_at)
                                    {
                                        const auto made{pixel_of(ink)};
                                        for(int y{}; y < height; ++ y)
                                        {
                                            for(int x{}; x < width; ++ x)
                                            {
                                                const auto image_x{box_x + x};
                                                const auto image_y{box_y + y};
                                                if(image_x < 0 || image_y < 0 || image_x >= selection.width() ||
                                                   image_y >= selection.height())
                                                {
                                                    continue;
                                                }
                                                auto amount{amount_at(x, y) * selection.at(image_x, image_y) / 255.0};
                                                if(!antialias)
                                                {
                                                    amount = amount >= 0.5 ? 1.0 : 0.0;
                                                }
                                                if(amount <= 0.0)
                                                {
                                                    continue;
                                                }
                                                const TilePoint point{image_x / tile_size, image_y / tile_size};
                                                auto& tile{layer.writable_tile(point)};
                                                const auto tile_x{image_x % tile_size};
                                                const auto tile_y{image_y % tile_size};
                                                tile.set_pixel(tile_x, tile_y,
                                                               blend(tile.template pixel<PixelType>(tile_x, tile_y),
                                                                     made, amount, BlendMode::normal));
                                                touched.insert(point);
                                            }
                                        }
                                    }};
                     const auto text_at{[&](int x, int y)
                                        {
                                            const auto value{static_cast<double>(coverage[cell(x, y)])};
                                            return antialias ? value : (value >= 0.5 ? 1.0 : 0.0);
                                        }};
                     if(effects.any())
                     {
                         // How far each pixel is from the text, as it shows without antialiasing: what goes around
                         // it is measured from there.
                         std::vector<bool> inside(coverage.size());
                         for(std::size_t index{}; index < coverage.size(); ++ index)
                         {
                             inside[index] = coverage[index] >= 0.5f;
                         }
                         const auto away{distances(inside, width, height)};
                         const auto away_at{[&](int x, int y)
                                            {
                                                return x < 0 || y < 0 || x >= width || y >= height
                                                           ? std::numeric_limits<double>::infinity()
                                                           : static_cast<double>(away[cell(x, y)]);
                                            }};
                         if(shadow > 0)
                         {
                             // The text and its outline, moved down and to the right, fading over `softness`
                             // pixels out from their edge.
                             mix(effects.shadow_color,
                                 [&](int x, int y)
                                 {
                                     const auto distance{away_at(x - shadow, y - shadow)};
                                     return std::clamp((outline + 0.5 + softness - distance) / softness, 0.0, 1.0);
                                 });
                         }
                         if(glow > 0)
                         {
                             mix(effects.glow_color,
                                 [&](int x, int y)
                                 {
                                     const auto fading{std::clamp(1.0 - (away_at(x, y) - outline) / glow, 0.0, 1.0)};
                                     return fading * fading;
                                 });
                         }
                         if(outline > 0)
                         {
                             mix(effects.outline_color,
                                 [&](int x, int y) {
                                     return std::max(std::clamp(outline + 0.5 - away_at(x, y), 0.0, 1.0),
                                                     text_at(x, y));
                                 });
                         }
                     }
                     mix(color, text_at);
                 });
    return {touched.begin(), touched.end()};
}

} // namespace oui_paint

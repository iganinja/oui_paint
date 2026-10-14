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
                                 const TextEffects& effects, const TextFace& face, double radians)
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

    // The glyphs as they are laid out, before they are turned.
    const auto laid_x{first_x};
    const auto laid_y{first_y};
    const auto laid_width{last_x - first_x + 1};
    const auto laid_height{last_y - first_y + 1};
    std::vector<float> laid(static_cast<std::size_t>(laid_width) * laid_height);
    const auto laid_cell{[laid_width](int x, int y) { return static_cast<std::size_t>(y) * laid_width + x; }};
    const auto add{[&](int x, int y, float amount)
                   {
                       auto& value{laid[laid_cell(x - laid_x, y - laid_y)]};
                       value = std::min(1.0f, std::max(value, amount));
                   }};
    each_glyph(font, text, left, top,
               [&](const auto& glyph, int x, int y, int baseline)
               {
                   for(int row{}; row < glyph.height; ++ row)
                   {
                       // A slanted row moves right by how far it is above the baseline, shared between the two
                       // columns it falls between; without antialiasing it moves whole, to the nearest column.
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
                               auto& here{laid[laid_cell(image_x - laid_x, y + row - laid_y)]};
                               here = std::min(1.0f, here + value * (1.0f - part));
                               auto& next{laid[laid_cell(image_x + 1 - laid_x, y + row - laid_y)]};
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
        const auto before{laid};
        for(int y{}; y < laid_height; ++ y)
        {
            for(int x{}; x < laid_width; ++ x)
            {
                auto& value{laid[laid_cell(x, y)]};
                for(int step{1}; step <= thicker && x - step >= 0; ++ step)
                {
                    value = std::max(value, before[laid_cell(x - step, y)]);
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

    // Turned round where the text starts, by `radians`, clockwise as the image shows it: each pixel takes what lies
    // under it once turned back, between the four pixels it falls among. Then room around it for what goes around it.
    const auto outline{std::max(effects.outline, 0)};
    const auto shadow{std::max(effects.shadow, 0)};
    const auto glow{std::max(effects.glow, 0)};
    const auto softness{std::max(1.0, shadow / 2.0)};
    const auto room{effects.any() ? std::max({outline + 2, outline + glow + 2,
                                              outline + shadow + static_cast<int>(std::ceil(softness)) + 2})
                                  : 0};
    const auto turned{radians != 0.0};
    const auto cosine{std::cos(radians)};
    const auto sine{std::sin(radians)};
    auto turned_x{laid_x};
    auto turned_y{laid_y};
    auto turned_right{laid_x + laid_width};
    auto turned_bottom{laid_y + laid_height};
    if(turned)
    {
        auto low_x{std::numeric_limits<double>::max()};
        auto low_y{std::numeric_limits<double>::max()};
        auto high_x{std::numeric_limits<double>::lowest()};
        auto high_y{std::numeric_limits<double>::lowest()};
        for(const auto& [cx, cy] : {std::pair{laid_x, laid_y}, std::pair{laid_x + laid_width, laid_y},
                                    std::pair{laid_x + laid_width, laid_y + laid_height},
                                    std::pair{laid_x, laid_y + laid_height}})
        {
            const auto dx{cx - at.x};
            const auto dy{cy - at.y};
            const auto x{at.x + dx * cosine - dy * sine};
            const auto y{at.y + dx * sine + dy * cosine};
            low_x = std::min(low_x, x);
            low_y = std::min(low_y, y);
            high_x = std::max(high_x, x);
            high_y = std::max(high_y, y);
        }
        turned_x = static_cast<int>(std::floor(low_x)) - 1;
        turned_y = static_cast<int>(std::floor(low_y)) - 1;
        turned_right = static_cast<int>(std::ceil(high_x)) + 1;
        turned_bottom = static_cast<int>(std::ceil(high_y)) + 1;
    }
    const auto box_x{turned_x - room};
    const auto box_y{turned_y - room};
    const auto width{turned_right - turned_x + 2 * room};
    const auto height{turned_bottom - turned_y + 2 * room};
    const auto cell{[width](int x, int y) { return static_cast<std::size_t>(y) * width + x; }};
    std::vector<float> coverage(static_cast<std::size_t>(width) * height);
    if(!turned)
    {
        for(int y{}; y < laid_height; ++ y)
        {
            std::copy_n(laid.begin() + static_cast<std::ptrdiff_t>(laid_cell(0, y)), laid_width,
                        coverage.begin() + static_cast<std::ptrdiff_t>(cell(room, y + room)));
        }
    }
    else
    {
        const auto laid_at{[&](int x, int y)
                           {
                               return x < 0 || y < 0 || x >= laid_width || y >= laid_height
                                          ? 0.0
                                          : static_cast<double>(laid[laid_cell(x, y)]);
                           }};
        for(int y{}; y < height; ++ y)
        {
            for(int x{}; x < width; ++ x)
            {
                // The middle of the pixel, turned back, as a place among the laid pixels' middles.
                const auto dx{box_x + x + 0.5 - at.x};
                const auto dy{box_y + y + 0.5 - at.y};
                const auto from_x{at.x + dx * cosine + dy * sine - laid_x - 0.5};
                const auto from_y{at.y - dx * sine + dy * cosine - laid_y - 0.5};
                const auto x0{static_cast<int>(std::floor(from_x))};
                const auto y0{static_cast<int>(std::floor(from_y))};
                const auto fx{from_x - x0};
                const auto fy{from_y - y0};
                const auto value{(laid_at(x0, y0) * (1 - fx) + laid_at(x0 + 1, y0) * fx) * (1 - fy) +
                                 (laid_at(x0, y0 + 1) * (1 - fx) + laid_at(x0 + 1, y0 + 1) * fx) * fy};
                coverage[cell(x, y)] = static_cast<float>(value);
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
                             // The text and its outline, moved away from the light, fading over `softness` pixels
                             // out from their edge: how far from the text, between the four pixels it falls among.
                             const auto light{effects.light * 3.14159265358979323846 / 180.0};
                             const auto shift_x{-std::cos(light) * shadow};
                             const auto shift_y{std::sin(light) * shadow};
                             const auto far{1e6};
                             const auto away_between{[&](double x, double y)
                                                     {
                                                         const auto x0{static_cast<int>(std::floor(x))};
                                                         const auto y0{static_cast<int>(std::floor(y))};
                                                         const auto fx{x - x0};
                                                         const auto fy{y - y0};
                                                         const auto at_cell{[&](int cx, int cy)
                                                                            { return std::min(away_at(cx, cy), far); }};
                                                         return (at_cell(x0, y0) * (1 - fx) + at_cell(x0 + 1, y0) * fx) *
                                                                    (1 - fy) +
                                                                (at_cell(x0, y0 + 1) * (1 - fx) +
                                                                 at_cell(x0 + 1, y0 + 1) * fx) *
                                                                    fy;
                                                     }};
                             mix(effects.shadow_color,
                                 [&](int x, int y)
                                 {
                                     const auto distance{away_between(x - shift_x, y - shift_y)};
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

std::array<Position, 4> text_corners(olagarro::oui::Font& font, std::string_view text, Position at, double radians)
{
    const auto lines{lines_of(font, text, static_cast<int>(std::lround(at.x)), static_cast<int>(std::lround(at.y)))};
    auto widest{0};
    for(const auto& line : lines)
    {
        widest = std::max(widest, line.right - line.left);
    }
    const auto width{static_cast<double>(widest)};
    const auto height{static_cast<double>(lines.size()) * font.line_height()};
    const auto cosine{std::cos(radians)};
    const auto sine{std::sin(radians)};
    const auto turned{[&](double dx, double dy)
                      { return Position{at.x + dx * cosine - dy * sine, at.y + dx * sine + dy * cosine}; }};
    return {turned(0, 0), turned(width, 0), turned(width, height), turned(0, height)};
}

} // namespace oui_paint

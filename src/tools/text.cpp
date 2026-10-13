#include "tools/text.h"

#include "core/blend.h"

#include <oui/utf8.h>

#include <cmath>
#include <set>

namespace oui_paint
{

std::vector<TilePoint> draw_text(Layer& layer, const Selection& selection, olagarro::oui::Font& font,
                                 std::string_view text, Position at, const Color& color, bool antialias)
{
    std::set<TilePoint> touched;
    const auto left{static_cast<int>(std::lround(at.x))};
    auto top{static_cast<int>(std::lround(at.y))};
    auto pen{left};
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
                     PixelType ink;
                     if constexpr(PixelType::channels == 2)
                     {
                         ink.value = {channel(0.299 * color.red + 0.587 * color.green + 0.114 * color.blue),
                                      channel(color.alpha)};
                     }
                     else
                     {
                         ink.value = {channel(color.red), channel(color.green), channel(color.blue),
                                      channel(color.alpha)};
                     }
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
                         for(int row{}; row < glyph.height; ++ row)
                         {
                             for(int column{}; column < glyph.width; ++ column)
                             {
                                 auto coverage{glyph.coverage[static_cast<std::size_t>(row) * glyph.width + column] /
                                               255.0};
                                 if(!antialias)
                                 {
                                     coverage = coverage >= 0.5 ? 1.0 : 0.0;
                                 }
                                 const auto x{pen + glyph.left + column};
                                 const auto y{baseline + glyph.top + row};
                                 auto amount{coverage * selection.at(x, y) / 255.0};
                                 if(!antialias)
                                 {
                                     amount = amount >= 0.5 ? 1.0 : 0.0;
                                 }
                                 if(amount <= 0.0 || x < 0 || y < 0 || x >= selection.width() ||
                                    y >= selection.height())
                                 {
                                     continue;
                                 }
                                 const TilePoint point{x / tile_size, y / tile_size};
                                 auto& tile{layer.writable_tile(point)};
                                 tile.set_pixel(x % tile_size, y % tile_size,
                                                blend(tile.template pixel<PixelType>(x % tile_size, y % tile_size),
                                                      ink, amount, BlendMode::normal));
                                 touched.insert(point);
                             }
                         }
                         pen += glyph.advance;
                     }
                 });
    return {touched.begin(), touched.end()};
}

} // namespace oui_paint

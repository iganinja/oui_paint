#pragma once

#include "core/document.h"
#include "effects/plugins.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// Image > Mode as plugins do it: every layer of a document into indices of one palette, or into another format, the
// layers worked on together as one image so a palette made fits them all.

// Every layer into indices of the palette `indexer` makes, of no more than `count` colours, or into `palette` when
// given, the document indexed after. Index 0 is kept see-through when any pixel is, half or more, leaving 255 colours;
// 256 otherwise. Throws when the indexer fails or the palette given has no room.
void index_document(PluginHost& host, const Indexer& indexer, const std::map<std::string, double>& values, int count,
                    const std::optional<std::vector<Color>>& palette, Document& document);

// A converter and the values of its parameters, one step of a change of format.
struct ConversionStep
{
    const Converter* converter{nullptr};
    std::map<std::string, double> values;
};

// Every layer through each step in turn, then kept as `format`, the document in that format after. Throws when a
// converter fails.
void convert_document(PluginHost& host, const std::vector<ConversionStep>& steps, PixelFormat format,
                      Document& document);

// The converters a change from `from` to `to` goes through, by what they convert to: "grayscale" from colour to grey,
// then "8 bits" from 16 bits a channel to 8; none for a change that loses nothing.
[[nodiscard]] std::vector<std::string> conversions_between(PixelFormat from, bool from_indexed, PixelFormat to);

} // namespace oui_paint

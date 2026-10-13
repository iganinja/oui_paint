#pragma once

#include "core/document.h"

#include <optional>
#include <string>
#include <string_view>

namespace oui_paint
{

// A document as an OpenRaster file: a zip with stack.xml naming the layers, top one first, each with its name,
// opacity, visibility and blend mode, a PNG of each at the document's depth, the merged image and a thumbnail.
// Paint.NET's modes OpenRaster has no name for are written with names of OUI Paint's own, which other programs read
// as normal.
[[nodiscard]] std::string write_openraster(const Document& document);
// A document from an OpenRaster file, at the deepest depth and in colour if any of its layers is; layers inside groups
// come out as layers of their own. None, with what went wrong in `error`, when it cannot be read.
[[nodiscard]] std::optional<Document> read_openraster(std::string_view bytes, std::string& error);

} // namespace oui_paint

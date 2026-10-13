#pragma once

#include <oui/completion.h>

#include <string>
#include <vector>

namespace oui_paint
{

// What plugins can use, as src/plugins/paint.d.ts declares it, for the workbench to suggest from.
[[nodiscard]] const olagarro::oui::ScriptApi& plugin_api();
// The words and pieces of JavaScript the workbench suggests besides the API: keywords, and loops and the like.
[[nodiscard]] const std::vector<olagarro::oui::Suggestion>& javascript_words();
// The methods and fields of an image, as a table of the help: how each is called and what it does.
[[nodiscard]] std::string image_methods_markdown();
// `markdown` with what lies between <!-- image-methods --> and <!-- /image-methods --> made that table.
[[nodiscard]] std::string with_image_methods(std::string markdown);

} // namespace oui_paint

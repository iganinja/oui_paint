#include "app/plugin_api.h"

#include <string_view>

namespace oui_paint
{

std::vector<std::string_view> embedded_plugin_api();

const olagarro::oui::ScriptApi& plugin_api()
{
    static const olagarro::oui::ScriptApi api{embedded_plugin_api().front()};
    return api;
}

const std::vector<olagarro::oui::Suggestion>& javascript_words()
{
    static const auto words{[]
                            {
                                std::vector<olagarro::oui::Suggestion> made{
                                    {"for", "loop", "for (let i = 0; i < $0; i++) {\n}"},
                                    {"forof", "loop over", "for (const $0 of ) {\n}"},
                                    {"if", "if", "if ($0) {\n}"},
                                    {"function", "function", "function $0() {\n}"},
                                    {"effect", "a new effect",
                                     "paint.effect({\n    name: \"$0\",\n    menu: \"Stylize\",\n    parameters: [],\n"
                                     "    render(image, values, context) {\n        return image;\n    },\n});"},
                                };
                                for(const auto* const word :
                                    {"async", "await", "break", "case", "catch", "class", "const", "continue",
                                     "default", "delete", "do", "else", "export", "extends", "finally", "import",
                                     "in", "instanceof", "let", "new", "of", "return", "static", "super", "switch",
                                     "throw", "try", "typeof", "var", "void", "while", "yield"})
                                {
                                    made.push_back({word, "keyword", {}});
                                }
                                for(const auto* const word : {"true", "false", "null", "undefined", "NaN", "Infinity",
                                                              "this", "Math", "Float32Array", "JSON"})
                                {
                                    made.push_back({word, "constant", {}});
                                }
                                return made;
                            }()};
    return words;
}

std::string image_methods_markdown()
{
    std::string markdown{"| Method | What it does |\n|---|---|\n"};
    const auto& api{plugin_api()};
    for(const auto& member : api.instance_members("paint.Image"))
    {
        const auto* const declared{api.declared("paint.Image#" + member.label)};
        if(declared == nullptr)
        {
            continue;
        }
        std::string doc;
        for(const auto& line : declared->doc)
        {
            doc += (doc.empty() ? "" : " ") + line;
        }
        markdown += "| `" + declared->text + "` | " + doc + " |\n";
    }
    return markdown;
}

std::string with_image_methods(std::string markdown)
{
    const std::string open{"<!-- image-methods -->"};
    const std::string close{"<!-- /image-methods -->"};
    const auto start{markdown.find(open)};
    const auto end{markdown.find(close)};
    if(start == std::string::npos || end == std::string::npos || end < start)
    {
        return markdown;
    }
    return markdown.replace(start, end + close.size() - start, image_methods_markdown());
}

} // namespace oui_paint

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// A JSON value: null, a boolean, a number, a string, an array or an object, as palette setups are written.
struct Json
{
    enum class Kind
    {
        null,
        boolean,
        number,
        string,
        array,
        object,
    };
    Kind kind{Kind::null};
    bool boolean{false};
    double number{0.0};
    std::string text;
    std::vector<Json> items;
    std::map<std::string, Json> members;

    // A member, null when there is none.
    [[nodiscard]] const Json& operator[](std::string_view name) const;
};

// The JSON in `text`, none when it is not JSON.
[[nodiscard]] std::optional<Json> parse_json(std::string_view text);
// A string as JSON writes it, quoted and escaped.
[[nodiscard]] std::string json_quoted(std::string_view text);

} // namespace oui_paint

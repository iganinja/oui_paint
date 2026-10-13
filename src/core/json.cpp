#include "core/json.h"

#include <cctype>
#include <cstdlib>

namespace oui_paint
{

namespace
{

class Parser
{
  public:
    explicit Parser(std::string_view text) : m_text{text} {}

    [[nodiscard]] std::optional<Json> document()
    {
        auto value{this->value()};
        skip();
        if(!value || m_at != m_text.size())
        {
            return std::nullopt;
        }
        return value;
    }

  private:
    void skip()
    {
        while(m_at < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_at])) != 0)
        {
            ++ m_at;
        }
    }

    [[nodiscard]] bool take(char character)
    {
        skip();
        if(m_at < m_text.size() && m_text[m_at] == character)
        {
            ++ m_at;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool word(std::string_view expected)
    {
        if(m_text.substr(m_at, expected.size()) == expected)
        {
            m_at += expected.size();
            return true;
        }
        return false;
    }

    [[nodiscard]] std::optional<std::string> string()
    {
        if(!take('"'))
        {
            return std::nullopt;
        }
        std::string result;
        while(m_at < m_text.size() && m_text[m_at] != '"')
        {
            auto character{m_text[m_at++]};
            if(character == '\\' && m_at < m_text.size())
            {
                const auto escaped{m_text[m_at++]};
                switch(escaped)
                {
                case 'n':
                    character = '\n';
                    break;
                case 't':
                    character = '\t';
                    break;
                case 'r':
                    character = '\r';
                    break;
                case 'u':
                {
                    // Only the code points of one UTF-16 unit, as palette names need.
                    if(m_at + 4 > m_text.size())
                    {
                        return std::nullopt;
                    }
                    const auto code{std::strtoul(std::string{m_text.substr(m_at, 4)}.c_str(), nullptr, 16)};
                    m_at += 4;
                    if(code < 0x80)
                    {
                        result += static_cast<char>(code);
                    }
                    else if(code < 0x800)
                    {
                        result += static_cast<char>(0xc0 | (code >> 6));
                        result += static_cast<char>(0x80 | (code & 0x3f));
                    }
                    else
                    {
                        result += static_cast<char>(0xe0 | (code >> 12));
                        result += static_cast<char>(0x80 | ((code >> 6) & 0x3f));
                        result += static_cast<char>(0x80 | (code & 0x3f));
                    }
                    continue;
                }
                default:
                    character = escaped;
                    break;
                }
            }
            result += character;
        }
        if(m_at >= m_text.size())
        {
            return std::nullopt;
        }
        ++ m_at;
        return result;
    }

    [[nodiscard]] std::optional<Json> value()
    {
        skip();
        if(m_at >= m_text.size())
        {
            return std::nullopt;
        }
        Json json;
        const auto first{m_text[m_at]};
        if(first == '"')
        {
            auto text{string()};
            if(!text)
            {
                return std::nullopt;
            }
            json.kind = Json::Kind::string;
            json.text = std::move(*text);
            return json;
        }
        if(first == '[')
        {
            ++ m_at;
            json.kind = Json::Kind::array;
            if(take(']'))
            {
                return json;
            }
            do
            {
                auto item{value()};
                if(!item)
                {
                    return std::nullopt;
                }
                json.items.push_back(std::move(*item));
            } while(take(','));
            return take(']') ? std::optional<Json>{std::move(json)} : std::nullopt;
        }
        if(first == '{')
        {
            ++ m_at;
            json.kind = Json::Kind::object;
            if(take('}'))
            {
                return json;
            }
            do
            {
                auto name{string()};
                if(!name || !take(':'))
                {
                    return std::nullopt;
                }
                auto member{value()};
                if(!member)
                {
                    return std::nullopt;
                }
                json.members[*name] = std::move(*member);
            } while(take(','));
            return take('}') ? std::optional<Json>{std::move(json)} : std::nullopt;
        }
        if(word("true") || word("false"))
        {
            json.kind = Json::Kind::boolean;
            json.boolean = m_text[m_at - 1] == 'e' && m_text[m_at - 2] == 'u';
            return json;
        }
        if(word("null"))
        {
            return json;
        }
        const std::string rest{m_text.substr(m_at, 64)};
        char* end{};
        json.number = std::strtod(rest.c_str(), &end);
        if(end == rest.c_str())
        {
            return std::nullopt;
        }
        m_at += static_cast<std::size_t>(end - rest.c_str());
        json.kind = Json::Kind::number;
        return json;
    }

    std::string_view m_text;
    std::size_t m_at{0};
};

} // namespace

const Json& Json::operator[](std::string_view name) const
{
    static const Json none;
    const auto found{members.find(std::string{name})};
    return found != members.end() ? found->second : none;
}

std::optional<Json> parse_json(std::string_view text)
{
    return Parser{text}.document();
}

std::string json_quoted(std::string_view text)
{
    std::string result{"\""};
    for(const auto character : text)
    {
        switch(character)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        default:
            result += character;
        }
    }
    return result + "\"";
}

} // namespace oui_paint

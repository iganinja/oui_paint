#include "app/open_document.h"

#include <oui/translation.h>

#include <oui/file_system.h>

#include <fstream>
#include <iterator>
#include <system_error>

namespace oui_paint
{

OpenDocument::OpenDocument(Document document) : m_document{std::move(document)}
{
    m_shown_width = m_document.width();
    m_shown_height = m_document.height();
    m_shown_palettes = m_document.palettes();
    m_viewport.set_image_size(m_shown_width, m_shown_height);
}

void OpenDocument::refresh(const std::vector<TilePoint>& tiles)
{
    if(m_document.width() != m_shown_width || m_document.height() != m_shown_height ||
       m_document.palettes() != m_shown_palettes)
    {
        m_shown_palettes = m_document.palettes();
        m_shown_width = m_document.width();
        m_shown_height = m_document.height();
        m_viewport.set_image_size(m_shown_width, m_shown_height);
        m_display.invalidate_all();
        return;
    }
    for(const auto& tile : tiles)
    {
        m_display.invalidate(tile);
    }
}

std::string OpenDocument::title() const
{
    if(m_path)
    {
        return olagarro::oui::utf8_of(m_path->filename());
    }
    return m_untitled_number == 1 ? olagarro::oui::translate("Documents", "Untitled")
                                   : olagarro::oui::format_text(olagarro::oui::translate("Documents", "Untitled {0}"),
                                                                {std::to_string(m_untitled_number)});
}

std::optional<std::string> read_file(const std::filesystem::path& path, std::string& error)
{
    std::ifstream file{path, std::ios::binary};
    if(!file)
    {
        error = olagarro::oui::format_text(olagarro::oui::translate("Files", "cannot open {0}"), {olagarro::oui::utf8_of(path)});
        return std::nullopt;
    }
    std::string bytes{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if(file.bad())
    {
        error = olagarro::oui::format_text(olagarro::oui::translate("Files", "cannot read {0}"), {olagarro::oui::utf8_of(path)});
        return std::nullopt;
    }
    return bytes;
}

bool write_file_atomically(const std::filesystem::path& path, std::string_view bytes, std::string& error)
{
    auto temporary{path};
    temporary += ".saving";
    {
        std::ofstream file{temporary, std::ios::binary | std::ios::trunc};
        if(!file)
        {
            error = olagarro::oui::format_text(olagarro::oui::translate("Files", "cannot write {0}"), {olagarro::oui::utf8_of(temporary)});
            return false;
        }
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if(!file)
        {
            error = olagarro::oui::format_text(olagarro::oui::translate("Files", "cannot write {0}"), {olagarro::oui::utf8_of(temporary)});
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }
    std::error_code failure;
    std::filesystem::rename(temporary, path, failure);
    if(failure)
    {
        error = olagarro::oui::format_text(olagarro::oui::translate("Files", "cannot replace {0}: {1}"), {olagarro::oui::utf8_of(path), failure.message()});
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

} // namespace oui_paint

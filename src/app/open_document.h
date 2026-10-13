#pragma once

#include "codecs/codecs.h"
#include "core/document.h"
#include "core/history.h"
#include "render/display_cache.h"
#include "render/viewport.h"

#include <filesystem>
#include <optional>
#include <string>

namespace oui_paint
{

// A document open in a tab: what is in it, how it shows, and the file it came from or was last saved to.
class OpenDocument
{
  public:
    explicit OpenDocument(Document document);
    OpenDocument(const OpenDocument&) = delete;
    OpenDocument& operator=(const OpenDocument&) = delete;

    [[nodiscard]] Document& document() { return m_document; }
    [[nodiscard]] const Document& document() const { return m_document; }
    [[nodiscard]] DisplayCache& display() { return m_display; }
    [[nodiscard]] Viewport& viewport() { return m_viewport; }
    [[nodiscard]] const Viewport& viewport() const { return m_viewport; }
    [[nodiscard]] History& history() { return m_history; }
    // Draws again the tiles that changed, and everything when the image's size changed.
    void refresh(const std::vector<TilePoint>& tiles);

    // The file, none until it is first saved.
    [[nodiscard]] const std::optional<std::filesystem::path>& path() const { return m_path; }
    void set_path(std::filesystem::path path) { m_path = std::move(path); }
    // The name the tab shows: the file's, or "Untitled" with a number.
    [[nodiscard]] std::string title() const;
    void set_untitled_number(int number) { m_untitled_number = number; }

    [[nodiscard]] bool modified() const { return m_modified; }
    void set_modified(bool modified) { m_modified = modified; }

  private:
    Document m_document;
    DisplayCache m_display{m_document};
    Viewport m_viewport;
    History m_history;
    // The size the view and the display were last given.
    int m_shown_width{0};
    int m_shown_height{0};
    std::optional<std::filesystem::path> m_path;
    int m_untitled_number{1};
    bool m_modified{false};
};

// The whole of a file, or an error saying why not.
[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path, std::string& error);
// Writes `bytes` to a file beside `path` and then puts it in its place, so a failure never leaves half a file.
[[nodiscard]] bool write_file_atomically(const std::filesystem::path& path, std::string_view bytes,
                                         std::string& error);

} // namespace oui_paint

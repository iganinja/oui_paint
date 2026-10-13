#pragma once

#include "core/document.h"
#include "ui/icons.h"

#include <oui/widget.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// The document's layers, top one first, as Paint.NET's Layers window shows them: an eye that shows or hides each, a
// small picture of it and its name, the active one marked; under them, buttons that add, remove, duplicate, merge
// and move layers and open their properties. It says what was asked; the program does it.
class LayersPanel final : public olagarro::oui::Widget
{
  public:
    static constexpr int width{24};

    enum class Action
    {
        add,
        remove,
        duplicate,
        merge_down,
        move_up,
        move_down,
        properties,
    };

    explicit LayersPanel(Icons& icons);

    // The document it shows, none for no document.
    void set_document(const Document* document);
    // Called with the layer clicked, to make it the active one.
    void on_select(std::function<void(std::size_t)> select);
    // Called with the layer whose eye was clicked.
    void on_toggle_visible(std::function<void(std::size_t)> toggle);
    void on_action(std::function<void(Action)> action);

    // The layer at a row counted from the panel's top, none past them.
    [[nodiscard]] std::optional<std::size_t> layer_at_row(int row) const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;
    [[nodiscard]] std::optional<std::string> tooltip_at(olagarro::oui::Point point) const override;

  private:
    // The cells the eye and the picture take at the start of each row.
    static constexpr int eye_width{2};
    static constexpr int picture_width{3};

    struct Button
    {
        Action action;
        std::string icon;
        std::string tooltip;
    };

    [[nodiscard]] std::optional<Action> button_at(olagarro::oui::Point cell) const;
    void paint_picture(olagarro::oui::Painter& painter, const Layer& layer, const olagarro::oui::Rect& cells) const;

    Icons& m_icons;
    const Document* m_document{};
    std::vector<Button> m_buttons;
    std::function<void(std::size_t)> m_select;
    std::function<void(std::size_t)> m_toggle_visible;
    std::function<void(Action)> m_action;
    mutable std::vector<std::uint32_t> m_picture;
};

} // namespace oui_paint

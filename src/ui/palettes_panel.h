#pragma once

#include "core/document.h"

#include <oui/widget.h>

#include <functional>
#include <optional>

namespace oui_paint
{

// An indexed image's palettes, a row each, the active layer's marked, as the colours its indices give: a click on a
// colour picks its index as the primary one, a right click as the secondary, a double click edits it, and a click on a
// row's number gives that palette to the active layer.
class PalettesPanel final : public olagarro::oui::Widget
{
  public:
    // Cells before a row's colours: its number.
    static constexpr int label_width{3};

    void set_document(const Document* document);
    void on_pick(std::function<void(int index, bool secondary)> pick);
    void on_edit(std::function<void(int slot, int index)> edit);
    void on_slot(std::function<void(int slot)> slot);

    // The palette and index at a cell inside the panel, the index none on a row's number.
    struct Place
    {
        int slot{0};
        std::optional<int> index;
    };
    [[nodiscard]] std::optional<Place> place_at(olagarro::oui::Point cell) const;

    [[nodiscard]] olagarro::oui::Size measure(const olagarro::oui::Constraints& constraints) const override;
    void paint(olagarro::oui::Painter& painter, const olagarro::oui::Theme& theme) const override;
    bool handle_event(const olagarro::oui::Event& event) override;

  private:
    // How many colours a row shows across, and so how many rows each palette takes.
    [[nodiscard]] int across() const;
    [[nodiscard]] int rows_per_palette() const;

    const Document* m_document{};
    std::function<void(int, bool)> m_pick;
    std::function<void(int, int)> m_edit;
    std::function<void(int)> m_slot;
};

} // namespace oui_paint

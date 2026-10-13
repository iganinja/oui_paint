#pragma once

#include "core/document.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// What a document's layers and selection were at some moment: the layers themselves, which share their tiles with the document
// until it writes to them, so keeping one costs a pointer per tile and only tiles written afterwards stay apart.
struct LayersState
{
    std::vector<Layer> layers;
    std::size_t active_layer{0};
    // The selection's mask, none when nothing is selected.
    std::optional<Layer> selection;
    int width{0};
    int height{0};

    [[nodiscard]] static LayersState of(const Document& document);
    void restore_into(Document& document) const;
};

// The tiles where two states of the same layers differ, layer by layer: what has to be drawn again after going from one
// to the other. A layer added or taken away differs everywhere it has tiles.
[[nodiscard]] std::vector<TilePoint> changed_tiles(const LayersState& before, const LayersState& after);

// Undo and redo for a document: each step keeps the layers before and after it.
class History
{
  public:
    struct Step
    {
        std::string name;
        LayersState before;
        LayersState after;
    };

    // Starts a step from the document as it is now; finish_step ends it.
    void begin_step(const Document& document);
    // Ends the step begun, named, keeping it when it changed anything; redo is lost.
    void finish_step(std::string name, const Document& document);
    // Ends the step begun leaving no trace, as when a stroke was cancelled.
    void cancel_step();
    [[nodiscard]] bool step_open() const { return m_open.has_value(); }

    [[nodiscard]] bool can_undo() const { return m_position > 0; }
    [[nodiscard]] bool can_redo() const { return m_position < m_steps.size(); }
    // Puts the document back as before the last step; returns the tiles that changed.
    std::vector<TilePoint> undo(Document& document);
    std::vector<TilePoint> redo(Document& document);
    [[nodiscard]] const std::vector<Step>& steps() const { return m_steps; }
    // How many steps are done: the rest are there to redo.
    [[nodiscard]] std::size_t position() const { return m_position; }

  private:
    std::vector<Step> m_steps;
    std::size_t m_position{0};
    std::optional<LayersState> m_open;
};

} // namespace oui_paint

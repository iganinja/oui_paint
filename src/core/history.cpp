#include "core/history.h"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace oui_paint
{

LayersState LayersState::of(const Document& document)
{
    return {document.layers(), document.active_layer(), document.selection().mask(), document.width(),
            document.height(),  document.palettes(), document.format()};
}

void LayersState::restore_into(Document& document) const
{
    if(width > 0 && (width != document.width() || height != document.height()))
    {
        document.set_size(width, height);
    }
    document.set_format(format);
    document.layers() = layers;
    document.selection().set_mask(selection);
    document.set_palettes(palettes);
    if(!layers.empty())
    {
        document.set_active_layer(std::min(active_layer, layers.size() - 1));
    }
}

std::vector<TilePoint> changed_tiles(const LayersState& before, const LayersState& after)
{
    std::set<TilePoint> changed;
    const auto count{std::max(before.layers.size(), after.layers.size())};
    for(std::size_t index{}; index < count; ++ index)
    {
        const auto* const old_layer{index < before.layers.size() ? &before.layers[index] : nullptr};
        const auto* const new_layer{index < after.layers.size() ? &after.layers[index] : nullptr};
        const auto shows_differently{old_layer == nullptr || new_layer == nullptr ||
                                     old_layer->palette_slot() != new_layer->palette_slot() ||
                                     old_layer->visible() != new_layer->visible() ||
                                     old_layer->opacity() != new_layer->opacity() ||
                                     old_layer->blend_mode() != new_layer->blend_mode()};
        for(const auto* const layer : {old_layer, new_layer})
        {
            if(layer == nullptr)
            {
                continue;
            }
            const auto* const other{layer == old_layer ? new_layer : old_layer};
            for(const auto& [point, tile] : layer->tiles())
            {
                if(shows_differently || other->tile(point) != tile)
                {
                    changed.insert(point);
                }
            }
        }
    }
    return {changed.begin(), changed.end()};
}

void History::begin_step(const Document& document)
{
    m_open = LayersState::of(document);
}

void History::finish_step(std::string name, const Document& document)
{
    if(!m_open)
    {
        return;
    }
    auto after{LayersState::of(document)};
    auto before{std::move(*m_open)};
    m_open.reset();
    const auto same_selection{before.selection.has_value() == after.selection.has_value() &&
                              (!before.selection || before.selection->tiles() == after.selection->tiles())};
    if(same_selection && before.format == after.format && before.palettes == after.palettes &&
       before.width == after.width && before.height == after.height &&
       changed_tiles(before, after).empty() && before.layers.size() == after.layers.size() &&
       before.active_layer == after.active_layer)
    {
        auto same_names{true};
        for(std::size_t index{}; index < before.layers.size(); ++ index)
        {
            same_names = same_names && before.layers[index].name() == after.layers[index].name();
        }
        if(same_names)
        {
            return;
        }
    }
    m_steps.erase(m_steps.begin() + static_cast<std::ptrdiff_t>(m_position), m_steps.end());
    m_steps.push_back({std::move(name), std::move(before), std::move(after)});
    m_position = m_steps.size();
}

void History::cancel_step()
{
    m_open.reset();
}

std::vector<TilePoint> History::undo(Document& document)
{
    if(!can_undo())
    {
        return {};
    }
    -- m_position;
    const auto& step{m_steps[m_position]};
    step.before.restore_into(document);
    return changed_tiles(step.after, step.before);
}

std::vector<TilePoint> History::redo(Document& document)
{
    if(!can_redo())
    {
        return {};
    }
    const auto& step{m_steps[m_position]};
    ++ m_position;
    step.after.restore_into(document);
    return changed_tiles(step.before, step.after);
}

std::vector<TilePoint> History::go_to(Document& document, std::size_t position)
{
    if(position > m_steps.size())
    {
        throw std::out_of_range{"no such step in the history"};
    }
    std::set<TilePoint> changed;
    while(m_position > position)
    {
        for(const auto& tile : undo(document))
        {
            changed.insert(tile);
        }
    }
    while(m_position < position)
    {
        for(const auto& tile : redo(document))
        {
            changed.insert(tile);
        }
    }
    return {changed.begin(), changed.end()};
}

} // namespace oui_paint

#include "ui/palettes_panel.h"

#include "ui/side_panel.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>
#include <cmath>
#include <string>

using namespace olagarro;

namespace oui_paint
{

void PalettesPanel::set_document(const Document* document)
{
    m_document = document;
}

void PalettesPanel::on_pick(std::function<void(int index, bool secondary)> pick)
{
    m_pick = std::move(pick);
}

void PalettesPanel::on_edit(std::function<void(int slot, int index)> edit)
{
    m_edit = std::move(edit);
}

void PalettesPanel::on_slot(std::function<void(int slot)> slot)
{
    m_slot = std::move(slot);
}

int PalettesPanel::across() const
{
    return std::max(1, m_bounds.width - label_width);
}

int PalettesPanel::rows_per_palette() const
{
    if(m_document == nullptr || !m_document->indexed())
    {
        return 0;
    }
    return (m_document->palettes()->size + across() - 1) / across();
}

std::optional<PalettesPanel::Place> PalettesPanel::place_at(oui::Point cell) const
{
    const auto rows{rows_per_palette()};
    if(rows == 0 || cell.y < 0 || cell.x < 0)
    {
        return std::nullopt;
    }
    const auto slot{cell.y / rows};
    if(slot >= m_document->palettes()->count())
    {
        return std::nullopt;
    }
    if(cell.x < label_width)
    {
        return Place{slot, std::nullopt};
    }
    const auto index{(cell.y % rows) * across() + cell.x - label_width};
    if(index >= m_document->palettes()->size)
    {
        return std::nullopt;
    }
    return Place{slot, index};
}

oui::Size PalettesPanel::measure(const oui::Constraints& constraints) const
{
    if(m_document == nullptr || !m_document->indexed())
    {
        return {0, 0};
    }
    // As wide as the layers above, and as many rows as the palettes take at that width.
    const auto width{std::min(constraints.max_width, 24)};
    const auto per_row{std::max(1, width - label_width)};
    const auto rows{(m_document->palettes()->size + per_row - 1) / per_row};
    return constraints.constrain({width, rows * m_document->palettes()->count()});
}

void PalettesPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    if(m_document == nullptr || !m_document->indexed())
    {
        return;
    }
    painter.fill_rect(m_bounds, side_panel_color(theme));
    const auto& setup{*m_document->palettes()};
    const auto active{m_document->layers().empty()
                          ? 0
                          : m_document->layers()[m_document->active_layer()].palette_slot()};
    const auto rows{rows_per_palette()};
    for(int slot{}; slot < setup.count(); ++ slot)
    {
        const auto top{m_bounds.y + slot * rows};
        const oui::TextStyle style{slot == active ? theme.menu_selected_foreground : theme.status_foreground,
                                   slot == active ? theme.menu_selected_background : side_panel_color(theme)};
        painter.fill_rect({m_bounds.x, top, label_width, rows}, style.background);
        painter.draw_text({m_bounds.x, top}, (slot == active ? ">" : " ") + std::to_string(slot + 1), style);
        const auto& palette{setup.palettes[static_cast<std::size_t>(slot)]};
        for(int index{}; index < setup.size; ++ index)
        {
            const oui::Rect cell{m_bounds.x + label_width + index % across(), top + index / across(), 1, 1};
            if(palette.transparent == index)
            {
                // See-through: a checker of two greys.
                painter.fill_rect(cell, {200, 200, 200});
                painter.draw_text({cell.x, cell.y}, "/", {{90, 90, 90}, {200, 200, 200}});
                continue;
            }
            const auto color{index < static_cast<int>(palette.colors.size()) ? palette.colors[static_cast<std::size_t>(index)]
                                                                             : Color{}};
            const auto byte{[](double value) { return static_cast<std::uint8_t>(std::lround(value * 255.0)); }};
            painter.fill_rect(cell, {byte(color.red), byte(color.green), byte(color.blue)});
        }
    }
}

bool PalettesPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed)
    {
        return false;
    }
    const auto place{place_at({mouse->x - m_bounds.x, mouse->y - m_bounds.y})};
    if(!place)
    {
        return false;
    }
    if(!place->index)
    {
        if(m_slot)
        {
            m_slot(place->slot);
        }
        return true;
    }
    if(mouse->clicks == 2 && !mouse->right && m_edit)
    {
        m_edit(place->slot, *place->index);
        return true;
    }
    if(m_pick)
    {
        m_pick(*place->index, mouse->right);
    }
    return true;
}

} // namespace oui_paint

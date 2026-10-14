#include "ui/layers_panel.h"

#include "ui/side_panel.h"

#include <oui/painter.h>
#include <oui/theme.h>

#include <algorithm>
#include <cmath>

using namespace olagarro;

namespace oui_paint
{

namespace
{

// A pixel of a layer as 0xAARRGGBB, for its small picture.
std::uint32_t screen_pixel(const Layer& layer, int x, int y)
{
    const auto tile{layer.tile({x / tile_size, y / tile_size})};
    if(tile == nullptr)
    {
        return 0;
    }
    return visit_format(layer.format(),
                        [&](auto tag) -> std::uint32_t
                        {
                            using PixelType = typename decltype(tag)::Type;
                            const auto pixel{tile->template pixel<PixelType>(x % tile_size, y % tile_size)};
                            const auto byte{[](auto channel)
                                            {
                                                return sizeof(channel) == 1
                                                           ? static_cast<std::uint32_t>(channel)
                                                           : static_cast<std::uint32_t>(channel) >> 8u;
                                            }};
                            if constexpr(PixelType::channels == 2)
                            {
                                const auto gray{byte(pixel.value[0])};
                                return byte(pixel.value[1]) << 24u | gray << 16u | gray << 8u | gray;
                            }
                            else
                            {
                                return byte(pixel.value[3]) << 24u | byte(pixel.value[0]) << 16u |
                                       byte(pixel.value[1]) << 8u | byte(pixel.value[2]);
                            }
                        });
}

} // namespace

LayersPanel::LayersPanel(Icons& icons) : m_icons{icons}
{
    m_buttons = {{Action::add, "Add", "Add New Layer (Ctrl+Shift+N)"},
                 {Action::remove, "Remove", "Delete Layer (Ctrl+Shift+Delete)"},
                 {Action::duplicate, "Duplicate", "Duplicate Layer (Ctrl+Shift+D)"},
                 {Action::merge_down, "MergeDown", "Merge Layer Down (Ctrl+M)"},
                 {Action::move_up, "MoveUp", "Move Layer Up"},
                 {Action::move_down, "MoveDown", "Move Layer Down"},
                 {Action::properties, "Tools", "Layer Properties (F4)"}};
}

void LayersPanel::set_document(const Document* document)
{
    m_document = document;
}

void LayersPanel::on_select(std::function<void(std::size_t)> select)
{
    m_select = std::move(select);
}

void LayersPanel::on_toggle_visible(std::function<void(std::size_t)> toggle)
{
    m_toggle_visible = std::move(toggle);
}

void LayersPanel::on_action(std::function<void(Action)> action)
{
    m_action = std::move(action);
}

std::optional<std::size_t> LayersPanel::layer_at_row(int row) const
{
    if(m_document == nullptr || row < 0 || row >= static_cast<int>(m_document->layers().size()))
    {
        return std::nullopt;
    }
    return m_document->layers().size() - 1 - static_cast<std::size_t>(row);
}

std::optional<LayersPanel::Action> LayersPanel::button_at(oui::Point cell) const
{
    // The buttons along the bottom row, two cells each.
    if(cell.y != m_bounds.height - 1 || cell.x < 0)
    {
        return std::nullopt;
    }
    const auto index{static_cast<std::size_t>(cell.x / 2)};
    if(index >= m_buttons.size())
    {
        return std::nullopt;
    }
    return m_buttons[index].action;
}

oui::Size LayersPanel::measure(const oui::Constraints& constraints) const
{
    return {std::min(width, constraints.max_width), constraints.max_height};
}

void LayersPanel::paint_picture(oui::Painter& painter, const Layer& layer, const oui::Rect& cells) const
{
    const auto area{painter.pixel_bounds(cells)};
    if(!area || m_document == nullptr || area->width <= 0 || area->height <= 0)
    {
        return;
    }
    // The layer fitted in the cells, over a checkerboard, each pixel of the picture the image's pixel at its middle.
    const auto scale{std::max(static_cast<double>(m_document->width()) / area->width,
                              static_cast<double>(m_document->height()) / area->height)};
    m_picture.assign(static_cast<std::size_t>(area->width) * area->height, 0x808080u);
    const auto shown_width{static_cast<int>(m_document->width() / scale)};
    const auto shown_height{static_cast<int>(m_document->height() / scale)};
    const auto left{(area->width - shown_width) / 2};
    const auto top{(area->height - shown_height) / 2};
    for(int y{}; y < shown_height; ++ y)
    {
        for(int x{}; x < shown_width; ++ x)
        {
            const auto image_x{std::min(m_document->width() - 1, static_cast<int>((x + 0.5) * scale))};
            const auto image_y{std::min(m_document->height() - 1, static_cast<int>((y + 0.5) * scale))};
            auto pixel{screen_pixel(layer, image_x, image_y)};
            if(m_document->indexed() && pixel >> 24u != 0)
            {
                // An index, as its palette's colour.
                const auto color{m_document->palettes()->color(layer.palette_slot(), static_cast<int>(pixel & 0xffu))};
                const auto byte{[](double value) { return static_cast<std::uint32_t>(std::lround(value * 255.0)); }};
                pixel = byte(color.alpha) << 24u | byte(color.red) << 16u | byte(color.green) << 8u | byte(color.blue);
            }
            const auto under{((x / 4 + y / 4) % 2 == 0) ? 0xffffffu : 0xccccccu};
            const auto alpha{pixel >> 24u};
            const auto mix{[alpha](std::uint32_t over, std::uint32_t base)
                           { return (over * alpha + base * (255u - alpha) + 127u) / 255u; }};
            m_picture[static_cast<std::size_t>((top + y) * area->width + left + x)] =
                mix(pixel >> 16u & 0xffu, under >> 16u & 0xffu) << 16u |
                mix(pixel >> 8u & 0xffu, under >> 8u & 0xffu) << 8u | mix(pixel & 0xffu, under & 0xffu);
        }
    }
    painter.draw_image(cells, {m_picture.data(), area->width, area->height, area->width}, {});
}

void LayersPanel::paint(oui::Painter& painter, const oui::Theme& theme) const
{
    painter.fill_rect(m_bounds, side_panel_color(theme));
    if(m_document != nullptr)
    {
        const auto& layers{m_document->layers()};
        for(int row{}; row < static_cast<int>(layers.size()) && row < m_bounds.height - 1; ++ row)
        {
            const auto index{*layer_at_row(row)};
            const auto& layer{layers[index]};
            const oui::Rect line{m_bounds.x, m_bounds.y + row, m_bounds.width, 1};
            const auto active{index == m_document->active_layer()};
            if(active)
            {
                painter.fill_rect(line, theme.menu_selected_background);
            }
            const oui::Rect eye{line.x, line.y, eye_width, 1};
            if(const auto area{painter.pixel_bounds(eye)})
            {
                if(const auto* const picture{m_icons.picture(
                       layer.visible() ? "GuiVisibilityVisible" : "GuiVisibilityHidden", area->width, area->height)})
                {
                    oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
                    image.alpha = true;
                    painter.draw_image(eye, image, theme);
                }
            }
            paint_picture(painter, layer, {line.x + eye_width, line.y, picture_width, 1});
            const oui::TextStyle style{active ? theme.menu_selected_foreground : theme.status_foreground,
                                       active ? theme.menu_selected_background : side_panel_color(theme)};
            painter.draw_label({line.x + eye_width + picture_width + 1, line.y}, layer.name(), style);
        }
    }
    // The buttons on a bar of their own, as a toolbar; none without a document to work on.
    if(m_document == nullptr || m_bounds.height < 2)
    {
        return;
    }
    const auto bottom{m_bounds.y + m_bounds.height - 1};
    painter.draw_divider({m_bounds.x, bottom, m_bounds.width, 1}, theme);
    for(std::size_t index{}; index < m_buttons.size(); ++ index)
    {
        const oui::Rect cell{m_bounds.x + static_cast<int>(index) * 2, bottom, 2, 1};
        if(const auto area{painter.pixel_bounds(cell)})
        {
            if(const auto* const picture{m_icons.picture(m_buttons[index].icon, area->width, area->height)})
            {
                oui::PixelImage image{picture->pixels.data(), picture->width, picture->height, picture->width};
                image.alpha = true;
                painter.draw_image(cell, image, theme);
            }
        }
    }
}

bool LayersPanel::handle_event(const oui::Event& event)
{
    const auto* const mouse{std::get_if<oui::MouseEvent>(&event)};
    if(mouse == nullptr || !mouse->pressed || mouse->right)
    {
        return false;
    }
    const oui::Point cell{mouse->x - m_bounds.x, mouse->y - m_bounds.y};
    if(const auto action{button_at(cell)})
    {
        if(m_action)
        {
            m_action(*action);
        }
        return true;
    }
    const auto index{layer_at_row(cell.y)};
    if(!index)
    {
        return false;
    }
    if(cell.x < eye_width)
    {
        if(m_toggle_visible)
        {
            m_toggle_visible(*index);
        }
        return true;
    }
    if(m_select)
    {
        m_select(*index);
    }
    if(mouse->clicks == 2 && m_action)
    {
        m_action(Action::properties);
    }
    return true;
}

std::optional<std::string> LayersPanel::tooltip_at(oui::Point point) const
{
    if(const auto action{button_at({point.x - m_bounds.x, point.y - m_bounds.y})})
    {
        for(const auto& button : m_buttons)
        {
            if(button.action == *action)
            {
                return button.tooltip;
            }
        }
    }
    return std::nullopt;
}

} // namespace oui_paint

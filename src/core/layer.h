#pragma once

#include "core/image.h"
#include "core/tile.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// How a layer's pixels mix with those under it: Paint.NET's modes.
enum class BlendMode
{
    normal,
    multiply,
    additive,
    color_burn,
    color_dodge,
    reflect,
    glow,
    overlay,
    difference,
    negation,
    lighten,
    darken,
    screen,
    xor_,
};

// Every mode, in the order Paint.NET lists them, and their names.
[[nodiscard]] const std::vector<BlendMode>& all_blend_modes();
[[nodiscard]] std::string_view blend_mode_name(BlendMode mode);

// One layer of a document: its pixels in tiles, and how it shows. A tile it does not have is see-through. Tiles are
// shared until written to: a copy of a layer, as undo keeps, costs the pointers to its tiles, and writing to a tile
// someone else holds too copies it first.
class Layer
{
  public:
    using Tiles = std::map<TilePoint, std::shared_ptr<Tile>>;

    Layer(PixelFormat format, std::string name);

    [[nodiscard]] PixelFormat format() const { return m_format; }
    [[nodiscard]] const std::string& name() const { return m_name; }
    void set_name(std::string name) { m_name = std::move(name); }
    [[nodiscard]] bool visible() const { return m_visible; }
    void set_visible(bool visible) { m_visible = visible; }
    // From 0, not shown, to 1.
    [[nodiscard]] double opacity() const { return m_opacity; }
    void set_opacity(double opacity);
    [[nodiscard]] BlendMode blend_mode() const { return m_blend_mode; }
    void set_blend_mode(BlendMode mode) { m_blend_mode = mode; }
    // In an indexed image, which palette its indices are of.
    [[nodiscard]] int palette_slot() const { return m_palette_slot; }
    void set_palette_slot(int slot) { m_palette_slot = slot; }
    // A layer with no pixels and the same name, showing and palette as this one.
    [[nodiscard]] Layer empty_like() const;

    // The tile at `point`, none where it is see-through.
    [[nodiscard]] std::shared_ptr<const Tile> tile(TilePoint point) const;
    // The tile at `point` to write to: made see-through where there was none, copied first when shared.
    [[nodiscard]] Tile& writable_tile(TilePoint point);
    // Puts a tile back as it was, or none, as undo does.
    void set_tile(TilePoint point, std::shared_ptr<const Tile> tile);
    [[nodiscard]] const Tiles& tiles() const { return m_tiles; }

    // Copies `image`, of this layer's format, in with its top left corner at x, y.
    void write(const Image& image, int x, int y);
    // The pixels of a rectangle, see-through where there are none.
    [[nodiscard]] Image read(int x, int y, int width, int height) const;

  private:
    PixelFormat m_format;
    std::string m_name;
    bool m_visible{true};
    double m_opacity{1.0};
    BlendMode m_blend_mode{BlendMode::normal};
    int m_palette_slot{0};
    Tiles m_tiles;
};

} // namespace oui_paint

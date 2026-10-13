#pragma once

#include "core/image.h"
#include "core/layer.h"
#include "core/palette.h"
#include "core/selection.h"
#include "core/transform.h"

#include <string>
#include <vector>

namespace oui_paint
{

// Where the image stays when the canvas grows or shrinks: one of nine places, as Paint.NET's anchor buttons.
struct Anchor
{
    // -1 left or top, 0 the middle, 1 right or bottom.
    int horizontal{0};
    int vertical{0};
};

// An image being edited: its size, its pixel format, which all its layers share, and its layers from the bottom up.
class Document
{
  public:
    Document(int width, int height, PixelFormat format);
    // A document of one layer holding `image`.
    [[nodiscard]] static Document from_image(const Image& image);

    [[nodiscard]] int width() const { return m_width; }
    [[nodiscard]] int height() const { return m_height; }
    [[nodiscard]] PixelFormat format() const { return m_format; }

    // An indexed image keeps an index in each pixel, as grey, and see-through where alpha is 0, of the palette of the
    // layer's slot. None for an image of colours or greys.
    [[nodiscard]] bool indexed() const { return m_palettes.has_value(); }
    [[nodiscard]] const std::optional<PaletteSetup>& palettes() const { return m_palettes; }
    void set_palettes(std::optional<PaletteSetup> palettes) { m_palettes = std::move(palettes); }
    // An indexed layer's pixels as the colours its palette gives them, in RGBA of 8 bits; others as they are.
    [[nodiscard]] Image colors_of(const Image& pixels, int slot) const;
    // Colours as the nearest indices of palette `slot`, for an indexed image; converted to its format otherwise.
    [[nodiscard]] Image indices_of(const Image& colors, int slot) const;

    [[nodiscard]] Selection& selection() { return m_selection; }
    [[nodiscard]] const Selection& selection() const { return m_selection; }
    [[nodiscard]] std::vector<Layer>& layers() { return m_layers; }
    [[nodiscard]] const std::vector<Layer>& layers() const { return m_layers; }
    [[nodiscard]] std::size_t active_layer() const { return m_active_layer; }
    void set_active_layer(std::size_t index);
    Layer& add_layer(std::string name);
    // A new see-through layer just above the active one, which it becomes.
    Layer& insert_layer_above(std::string name);
    // Takes the layer away; the one under it, or the new bottom one, becomes active. The last layer stays.
    void remove_layer(std::size_t index);
    // A copy just above it, sharing its tiles until either is written, which becomes active.
    void duplicate_layer(std::size_t index);
    // Moves a layer to another place among them; it stays active if it was.
    void move_layer(std::size_t from, std::size_t to);
    // Mixes the layer into the one under it as it shows, by its mode and opacity, and takes it away; the one under it
    // becomes active. Nothing for the bottom layer.
    void merge_down(std::size_t index);
    // All visible layers as one.
    void flatten_layers();
    // A bigger or smaller canvas with the image where it was, at the top left: pixels past the new edges are kept in
    // their tiles until written over, as layers keep what moves outside them.
    void set_size(int width, int height);
    // The canvas cut down to the selection's bounds, every layer with it; where the selection is not a rectangle,
    // what it leaves out becomes see-through. Nothing is selected afterwards.
    void crop_to_selection();

    // The whole image, every layer and the selection.
    void flip(bool horizontal);
    void rotate(int quarter_turns);
    void resize(int width, int height, Resampling resampling);
    // A new canvas size, the image placed by `anchor`; what falls outside is cut off, what is new is see-through.
    void resize_canvas(int width, int height, Anchor anchor);
    // One layer alone.
    void flip_layer(std::size_t index, bool horizontal);
    // A layer turned by `degrees` clockwise round the image's middle, then made `zoom` times as big and moved by `pan`
    // pixels, as Paint.NET's Rotate / Zoom does.
    void rotate_zoom_layer(std::size_t index, double degrees, double zoom, double pan_x, double pan_y,
                           Resampling resampling);

    // What the document looks like: the visible layers mixed from the bottom up, in its format, or in RGBA of 8 bits
    // for an indexed image.
    [[nodiscard]] Image flatten() const;

  private:
    // Every layer and the selection's mask made anew by `change`, at a new size.
    template <typename Change> void remake(int width, int height, Change change);

    int m_width;
    int m_height;
    PixelFormat m_format;
    std::vector<Layer> m_layers;
    std::size_t m_active_layer{0};
    Selection m_selection;
    std::optional<PaletteSetup> m_palettes;
};

} // namespace oui_paint

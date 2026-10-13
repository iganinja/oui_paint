#pragma once

namespace oui_paint
{

// A point in pixels, of the image or of the view, between whole pixels too.
struct Position
{
    double x{0.0};
    double y{0.0};
};

// Where the image is in the view and how big: its zoom, and where its top left corner lands. An image smaller than
// the view sits in its middle; a bigger one scrolls no further than its edges.
class Viewport
{
  public:
    // Paint.NET's zoom steps, from 1% to 3200%.
    static constexpr double zoom_steps[]{0.01, 0.02, 0.03, 0.04, 0.05, 0.06, 0.08, 0.12, 0.16, 0.25, 0.33, 0.50, 0.66,
                                         1.0,  1.5,  2.0,  3.0,  4.0,  5.0,  6.0,  7.0,  8.0,  12.0, 16.0, 24.0, 32.0};

    void set_image_size(int width, int height);
    void set_view_size(int width, int height);
    [[nodiscard]] int view_width() const { return m_view_width; }
    [[nodiscard]] int view_height() const { return m_view_height; }

    [[nodiscard]] double zoom() const { return m_zoom; }
    // Keeps the image point under `anchor`, a point of the view, where it is.
    void set_zoom(double zoom, Position anchor);
    void zoom_in(Position anchor);
    void zoom_out(Position anchor);
    // The biggest step at which the whole image fits, never above 100%.
    void zoom_to_fit();
    // As big as the area of the image from x, y fits in the view, by the zoom steps, with it in the middle as far as the image's
    // edges allow, as Zoom to Selection shows it.
    void zoom_to(double x, double y, double width, double height);
    void scroll_by(double dx, double dy);

    // Where the image's top left corner is in the view.
    [[nodiscard]] Position origin() const { return m_origin; }
    [[nodiscard]] Position image_at(Position view) const;
    [[nodiscard]] Position view_at(Position image) const;

  private:
    void keep_in_bounds();

    int m_image_width{1};
    int m_image_height{1};
    int m_view_width{1};
    int m_view_height{1};
    double m_zoom{1.0};
    Position m_origin;
};

} // namespace oui_paint

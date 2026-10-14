#include "ui/transform_dialogs.h"

#include "ui/anchor_grid.h"

#include "ui/shown_names.h"

#include <oui/translation.h>
#include <oui/button.h>
#include <oui/check_button.h>
#include <oui/combo_box.h>
#include <oui/grid.h>
#include <oui/label.h>
#include <oui/layout.h>
#include <oui/line_edit.h>
#include <oui/number_field.h>
#include <oui/slider.h>
#include <oui/window.h>

#include <cmath>
#include <string>
#include <vector>

using namespace olagarro;

namespace oui_paint
{

namespace
{

// A form of labels and what they name, a row each, and OK and Cancel under it.
class Form
{
  public:
    explicit Form(std::string_view title) : m_window{oui::window(title)}
    {
        m_content = &oui::column(*m_window);
        m_content->set_gap(oui::Spacing::tight);
        m_grid = &oui::grid(*m_content);
        m_grid->set_columns({oui::Length::content(), oui::Length::content(), oui::Length::content()});
        m_grid->set_gap(oui::Spacing::tight);
    }

    oui::NumberField& number(std::string_view name, double minimum, double maximum, double value,
                             std::string_view after = "")
    {
        auto& label{oui::label(name, *m_grid)};
        auto& field{oui::number_field(*m_grid)};
        field.set_range(minimum, maximum);
        field.set_value(value);
        label.set_buddy(field.text_field());
        oui::label(after, *m_grid);
        return field;
    }

    // A widget of its own under the rows before, in the second column.
    template <typename Child> Child& below(std::unique_ptr<Child> child)
    {
        oui::label("", *m_grid);
        auto& added{m_grid->add(std::move(child))};
        oui::label("", *m_grid);
        return added;
    }

    oui::ComboBox& choice(std::string_view name, std::vector<std::string> options, std::size_t selected = 0)
    {
        auto& label{oui::label(name, *m_grid)};
        auto& combo{m_grid->add(std::make_unique<oui::ComboBox>(std::move(options), selected))};
        label.set_buddy(combo);
        oui::label("", *m_grid);
        return combo;
    }

    oui::CheckButton& check(std::string_view name, bool checked)
    {
        oui::label("", *m_grid);
        auto& box{oui::check_button(name, *m_grid)};
        box.set_checked(checked);
        oui::label("", *m_grid);
        return box;
    }

    // Shows it; true when accepted. `before` runs on OK, before it closes.
    bool ask(oui::Application& application, const std::function<void()>& before = {})
    {
        auto& buttons{oui::row(*m_content)};
        buttons.set_gap(oui::Spacing::normal);
        auto& window{*m_window};
        auto accepted{false};
        oui::button(oui::translate("Dialogs", "OK"), buttons,
                    [&]
                    {
                        if(before)
                        {
                            before();
                        }
                        accepted = true;
                        window.close(oui::DialogResult::accepted);
                    }).set_default(true);
        oui::button(oui::translate("Dialogs", "Cancel"), buttons, [&] { window.close(oui::DialogResult::cancelled); });
        static_cast<void>(application.show_modal_and_keep(m_window));
        return accepted;
    }

  private:
    std::unique_ptr<oui::Window> m_window;
    oui::Column* m_content{};
    oui::Grid* m_grid{};
};

std::vector<std::string> resampling_names()
{
    std::vector<std::string> names;
    for(const auto resampling : all_resamplings())
    {
        names.push_back(shown_name(resampling));
    }
    return names;
}

// Width and height that keep their proportions while `keep` is checked: changing one changes the other.
void keep_proportions(oui::NumberField& width, oui::NumberField& height, oui::CheckButton& keep, double ratio,
                      bool& updating)
{
    static_cast<void>(width.on_changed(
        [&, ratio](double value)
        {
            if(!updating && keep.is_checked())
            {
                updating = true;
                height.set_value(std::max(1.0, std::round(value / ratio)));
                updating = false;
            }
        }));
    static_cast<void>(height.on_changed(
        [&, ratio](double value)
        {
            if(!updating && keep.is_checked())
            {
                updating = true;
                width.set_value(std::max(1.0, std::round(value * ratio)));
                updating = false;
            }
        }));
}

} // namespace

std::optional<ResizeRequest> ask_resize(oui::Application& application, int width, int height)
{
    Form form{oui::translate("Dialogs", "Resize Image")};
    auto& resampling{form.choice(oui::translate("Dialogs", "&Resampling:"), resampling_names())};
    auto& percent{form.number(oui::translate("Dialogs", "By &percentage:"), 1, 10000, 100, "%")};
    auto& keep{form.check(oui::translate("Dialogs", "&Maintain aspect ratio"), true)};
    auto& new_width{form.number(oui::translate("Dialogs", "&Width:"), 1, 65535, width, oui::translate("Dialogs", "pixels"))};
    auto& new_height{form.number(oui::translate("Dialogs", "&Height:"), 1, 65535, height, oui::translate("Dialogs", "pixels"))};
    auto updating{false};
    keep_proportions(new_width, new_height, keep, static_cast<double>(width) / height, updating);
    static_cast<void>(percent.on_changed(
        [&](double value)
        {
            if(updating)
            {
                return;
            }
            updating = true;
            new_width.set_value(std::max(1.0, std::round(width * value / 100.0)));
            new_height.set_value(std::max(1.0, std::round(height * value / 100.0)));
            updating = false;
        }));
    if(!form.ask(application,
                 [&]
                 {
                     new_width.commit();
                     new_height.commit();
                 }))
    {
        return std::nullopt;
    }
    return ResizeRequest{static_cast<int>(new_width.value()), static_cast<int>(new_height.value()),
                         all_resamplings()[resampling.selected()]};
}

std::optional<CanvasRequest> ask_canvas_size(oui::Application& application, Icons& icons, int width, int height)
{
    Form form{oui::translate("Dialogs", "Canvas Size")};
    auto& keep{form.check(oui::translate("Dialogs", "&Maintain aspect ratio"), false)};
    auto& new_width{form.number(oui::translate("Dialogs", "&Width:"), 1, 65535, width, oui::translate("Dialogs", "pixels"))};
    auto& new_height{form.number(oui::translate("Dialogs", "&Height:"), 1, 65535, height, oui::translate("Dialogs", "pixels"))};
    auto updating{false};
    keep_proportions(new_width, new_height, keep, static_cast<double>(width) / height, updating);
    auto& anchor{form.choice(oui::translate("Dialogs", "&Anchor:"),
                             {oui::translate("Dialogs", "Top Left"), oui::translate("Dialogs", "Top"), oui::translate("Dialogs", "Top Right"), oui::translate("Dialogs", "Left"), oui::translate("Dialogs", "Middle"), oui::translate("Dialogs", "Right"), oui::translate("Dialogs", "Bottom Left"), oui::translate("Dialogs", "Bottom"),
                              oui::translate("Dialogs", "Bottom Right")},
                             4)};
    // The anchor as Paint.NET shows it too: the image in its square, arrows where the canvas grows; each follows the
    // other.
    auto& grid{form.below(std::make_unique<AnchorGrid>(icons, 4))};
    grid.on_changed([&anchor](int chosen) { anchor.set_selected(static_cast<std::size_t>(chosen)); });
    static_cast<void>(anchor.on_selected([&grid](std::size_t chosen) { grid.set_anchor(static_cast<int>(chosen)); }));
    if(!form.ask(application,
                 [&]
                 {
                     new_width.commit();
                     new_height.commit();
                 }))
    {
        return std::nullopt;
    }
    const auto place{static_cast<int>(anchor.selected())};
    return CanvasRequest{static_cast<int>(new_width.value()), static_cast<int>(new_height.value()),
                         {place % 3 - 1, place / 3 - 1}};
}

std::optional<RotateZoomRequest> ask_rotate_zoom(oui::Application& application)
{
    Form form{oui::translate("Dialogs", "Rotate / Zoom Layer")};
    auto& angle{form.number(oui::translate("Dialogs", "&Angle:"), -180, 180, 0, oui::translate("Dialogs", "degrees"))};
    angle.set_step(0);
    auto& zoom{form.number(oui::translate("Dialogs", "&Zoom:"), 1, 10000, 100, "%")};
    zoom.set_step(0);
    auto& pan_x{form.number(oui::translate("Dialogs", "Pan &X:"), -65535, 65535, 0, oui::translate("Dialogs", "pixels"))};
    auto& pan_y{form.number(oui::translate("Dialogs", "Pan &Y:"), -65535, 65535, 0, oui::translate("Dialogs", "pixels"))};
    if(!form.ask(application,
                 [&]
                 {
                     angle.commit();
                     zoom.commit();
                     pan_x.commit();
                     pan_y.commit();
                 }))
    {
        return std::nullopt;
    }
    return RotateZoomRequest{angle.value(), zoom.value() / 100.0, pan_x.value(), pan_y.value()};
}

} // namespace oui_paint

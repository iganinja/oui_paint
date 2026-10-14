#include "app/translations.h"

namespace oui_paint
{

std::vector<std::string_view> embedded_catalogs();

const std::vector<olagarro::oui::Translation>& paint_translations()
{
    static const auto translations{olagarro::oui::embedded_translations(embedded_catalogs())};
    return translations;
}

std::string install_paint_language(std::string_view setting)
{
    const auto language{setting == "auto" || setting.empty() ? olagarro::oui::system_language() : std::string{setting}};
    olagarro::oui::install_language(language, paint_translations());
    return language;
}

std::string plugin_text(std::string_view text)
{
    return olagarro::oui::translate("Plugins", text);
}

} // namespace oui_paint

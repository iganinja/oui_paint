#pragma once

#include <oui/application.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace oui_paint
{

// A plugin as it loaded: its file's name, the file for one of the person's own, none for one built in, its code, the
// effects it declared, and why it failed, when it did.
struct PluginRecord
{
    std::string name;
    std::optional<std::filesystem::path> file;
    std::string source;
    std::vector<std::string> effects;
    std::optional<std::string> error;
};

// What was chosen in Manage Plugins.
struct PluginsChoice
{
    enum class Action
    {
        close,
        new_plugin,
        edit,
        reload
    };
    Action action{Action::close};
    // The plugin to edit.
    std::size_t plugin{};
};

// Lists the plugins, built in and the person's own, with what each declares or why it failed; Open Folder opens the
// plugins folder there and then, the rest are told back.
[[nodiscard]] PluginsChoice ask_manage_plugins(olagarro::oui::Application& application,
                                               const std::vector<PluginRecord>& plugins,
                                               const std::filesystem::path& folder);

} // namespace oui_paint

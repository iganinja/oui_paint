#pragma once

#include <oui/application.h>
#include <oui/process.h>
#include <oui/settings.h>

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace oui_paint
{

// Help > Check for Updates: asks GitHub for the latest release, and when it is newer, installs it in place of this
// one where install.sh and install.ps1 put it, says how to update it where APT put it, and anywhere else gives the
// release's page. What fetches is curl, wget or PowerShell, whichever the system has: OUI Paint carries no code of its
// own for the network. OUI Code has the same, written for itself, which this was copied from.

// What is looked for: whose releases, the file of them that is this program, the version this is, and where this one
// runs from.
struct UpdateProgram
{
    // The name shown, as "OUI Paint".
    std::string title;
    // "iganinja/oui_paint".
    std::string repository;
    // The release's file that is this program, as "oui_paint-linux-x86_64".
    std::string asset;
    // This one's version, as "0.4.7".
    std::string version;
    std::filesystem::path executable;
    // Where GitHub answers, which tests change: its API, and where releases' files and pages are.
    std::string api{"https://api.github.com"};
    std::string site{"https://github.com"};
};

// OUI Paint as it was built and runs.
[[nodiscard]] UpdateProgram oui_paint_update_program();

// A version's three numbers, from "v0.4.7", "0.4.7" or "0.4.7+3.ged0fda3"; none when it is not one.
[[nodiscard]] std::optional<std::array<int, 3>> parse_version(std::string_view text);
// Whether `latest` is a later version than `current`; false when either is not a version.
[[nodiscard]] bool is_newer(std::string_view latest, std::string_view current);
// The tag of the release GitHub's API describes, from its "tag_name"; none when the answer has none.
[[nodiscard]] std::optional<std::string> release_tag(std::string_view answer);
// The sum SHA256SUMS lists for `file`, in lower case hexadecimal; none when it lists none.
[[nodiscard]] std::optional<std::string> listed_sum(std::string_view sums, std::string_view file);
// The SHA-256 of `bytes`, in lower case hexadecimal.
[[nodiscard]] std::string sha256(std::string_view bytes);

// How this copy was installed, which says what updating it takes.
enum class Installation
{
    // By a package manager, APT, in /usr/bin: it updates it.
    package,
    // By install.sh in ~/.local/bin or install.ps1 in %LOCALAPPDATA%\Programs\<folder>: it can be replaced.
    standalone,
    // Anywhere else, as a build or a package unpacked: only the release's page is given.
    other
};
// `home` and `local_app_data` are where the installers put it from, the HOME and LOCALAPPDATA of the system; `name`
// is its file's, as "oui_paint", and `folder` is install.ps1's, as "oui_paint".
[[nodiscard]] Installation installation_of(const std::filesystem::path& executable, const std::filesystem::path& home,
                                           const std::filesystem::path& local_app_data, std::string_view name,
                                           std::string_view folder);
// The program running, its links followed; empty when the system does not say.
[[nodiscard]] std::filesystem::path running_executable();

// The command that downloads `url` to `to`, with the first of what can that is in `path`, the PATH as the system
// gives it: curl, then wget, or on Windows curl and then PowerShell. None when there is none of them.
[[nodiscard]] std::optional<std::vector<std::string>>
fetch_command(std::string_view url, const std::filesystem::path& to, std::string_view path);
// Puts `downloaded` in place of `executable`, runnable, which a system lets do while it runs, Windows once the one
// running is renamed: it is left as <name>.old, which remove_replaced takes away the next time. A message saying what
// failed, none when it is done.
[[nodiscard]] std::optional<std::string> replace_executable(const std::filesystem::path& executable,
                                                            const std::filesystem::path& downloaded);
// Takes away the <name>.old a replacement left on Windows, now that it no longer runs.
void remove_replaced(const std::filesystem::path& executable);

// Help > Check for Updates: says what it is doing while it asks, and what it found, and installs it if asked.
void check_for_updates(olagarro::oui::Application& application, olagarro::oui::Settings& settings,
                       const UpdateProgram& program);

// The check made as the program starts, when Preferences ask for it and a day has gone since the last: it says nothing
// unless there is a newer version, which it then offers as Check for Updates does. Destroying it stops it.
class StartupUpdateCheck
{
  public:
    StartupUpdateCheck(olagarro::oui::Application& application, olagarro::oui::Settings& settings,
                       UpdateProgram program);
    StartupUpdateCheck(const StartupUpdateCheck&) = delete;
    StartupUpdateCheck& operator=(const StartupUpdateCheck&) = delete;
    ~StartupUpdateCheck();

    // Starts it if Preferences ask and it is time, at `now` seconds since 1970. `found` hears a newer version's tag;
    // none means offering it, as the program does.
    void start(long long now, std::function<void(const std::string& tag)> found = {});
    [[nodiscard]] bool is_running() const;

  private:
    olagarro::oui::Application& m_application;
    olagarro::oui::Settings& m_settings;
    UpdateProgram m_program;
    std::filesystem::path m_answer;
    std::unique_ptr<olagarro::oui::Process> m_process;
};

// Whether a check is due at `now`, seconds since 1970: Preferences ask for it and a day has gone since the last.
[[nodiscard]] bool update_check_due(const olagarro::oui::Settings& settings, long long now);
// Says a check was made at `now`.
void remember_update_check(olagarro::oui::Settings& settings, long long now);

} // namespace oui_paint

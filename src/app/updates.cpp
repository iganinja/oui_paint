#include "app/updates.h"

#include "app/help.h"

#include <oui/oui.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

using namespace olagarro;

namespace oui_paint
{

namespace
{

// Where install.sh and install.ps1 put it, as the file is named, and the folder install.ps1 makes.
constexpr std::string_view program_name{"oui_paint"};

// Between the folders of PATH.
#ifdef _WIN32
constexpr char separator{';'};
#else
constexpr char separator{':'};
#endif

[[nodiscard]] std::string environment(const char* name)
{
    const auto* const value{std::getenv(name)};
    return value != nullptr ? std::string{value} : std::string{};
}

[[nodiscard]] std::string version_of_tag(std::string_view tag)
{
    return std::string{!tag.empty() && tag.front() == 'v' ? tag.substr(1) : tag};
}

[[nodiscard]] std::string latest_url(const UpdateProgram& program)
{
    return program.api + "/repos/" + program.repository + "/releases/latest";
}

[[nodiscard]] std::string release_file_url(const UpdateProgram& program, std::string_view tag, std::string_view file)
{
    return program.site + "/" + program.repository + "/releases/download/" + std::string{tag} + "/" + std::string{file};
}

[[nodiscard]] std::string release_page(const UpdateProgram& program, std::string_view tag)
{
    return program.site + "/" + program.repository + "/releases" + (tag.empty() ? "" : "/tag/" + std::string{tag});
}

[[nodiscard]] std::optional<std::string> read_file(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    if(!file)
    {
        return std::nullopt;
    }
    std::ostringstream bytes;
    bytes << file.rdbuf();
    return std::move(bytes).str();
}

// A file of its own to download into, in the system's temporary folder.
[[nodiscard]] std::filesystem::path temporary_file(std::string_view what)
{
    std::error_code ignored;
    const auto folder{std::filesystem::temp_directory_path(ignored)};
    const auto stamp{std::chrono::steady_clock::now().time_since_epoch().count()};
    return folder / oui::path_from_utf8(std::string{program_name} + "-update-" + std::string{what} + "-" +
                                        std::to_string(stamp));
}

void remove_quietly(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

[[nodiscard]] long long seconds_now()
{
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// What a download ended with: the downloader's exit code, -1 when it could not start, and what it wrote as errors.
struct Fetched
{
    int code{};
    std::string error;
};

// Downloads `url` to `to` while a window says `doing`, which Esc closes to stop it: none then.
[[nodiscard]] std::optional<Fetched> fetch_while_shown(oui::Application& application, const std::string& doing,
                                                       const std::string& url, const std::filesystem::path& to)
{
    const auto command{fetch_command(url, to, environment("PATH"))};
    if(!command)
    {
        return Fetched{-1, oui::translate("Updates", "There is no curl, wget or PowerShell here to download with.")};
    }
    auto dialog{oui::window(oui::translate("Updates", "Check for Updates"))};
    auto& content{oui::column(*dialog)};
    content.set_gap(oui::Spacing::tight);
    oui::label(doing, content);
    oui::label(oui::translate("Updates", "Esc stops it."), content);
    auto& dialog_reference{*dialog};
    std::string error;
    std::optional<int> exit_code;
    std::unique_ptr<oui::Process> process;
    try
    {
        process = oui::start_process(application, {*command, {}, {}},
                                     {[](std::string_view) {}, [&error](std::string_view text) { error += text; },
                                      [&exit_code, &dialog_reference](int code)
                                      {
                                          exit_code = code;
                                          dialog_reference.close(oui::DialogResult::accepted);
                                      }});
    }
    catch(const std::system_error& failure)
    {
        return Fetched{-1, failure.what()};
    }
    static_cast<void>(application.show_modal(std::move(dialog)));
    if(!exit_code)
    {
        return std::nullopt;
    }
    return Fetched{*exit_code, error};
}

// Says what failed, and offers the releases' page, where it can be had by hand.
void show_failure(oui::Application& application, const UpdateProgram& program, const std::string& what,
                  const std::string& detail, std::string_view tag = {})
{
    constexpr std::size_t most_shown{600};
    auto message{what};
    if(!detail.empty())
    {
        message += "\n" + detail.substr(0, most_shown);
    }
    const auto chosen{
        oui::message_box(application, oui::translate("Updates", "Check for Updates"), message,
                         {oui::translate("Updates", "OK"), oui::translate("Updates", "Open the Release's Page")})};
    if(chosen == std::size_t{1})
    {
        static_cast<void>(oui::open_externally(release_page(program, tag)));
    }
}

// Downloads the release of `tag`, checks it against its SHA256SUMS and puts it in place of the one running.
void install_update(oui::Application& application, const UpdateProgram& program, const std::string& tag)
{
    const auto latest{version_of_tag(tag)};
    const auto doing{oui::format_text(oui::translate("Updates", "Downloading {0} {1}..."), {program.title, latest})};
    const auto sums_file{temporary_file("sums")};
    const auto sums_fetched{
        fetch_while_shown(application, doing, release_file_url(program, tag, "SHA256SUMS"), sums_file)};
    const auto sums{read_file(sums_file)};
    remove_quietly(sums_file);
    if(!sums_fetched)
    {
        return;
    }
    const auto cannot{
        oui::format_text(oui::translate("Updates", "{0} {1} could not be downloaded."), {program.title, latest})};
    if(sums_fetched->code != 0 || !sums)
    {
        show_failure(application, program, cannot, sums_fetched->error, tag);
        return;
    }
    const auto sum{listed_sum(*sums, program.asset)};
    if(!sum)
    {
        show_failure(application, program, cannot,
                     oui::format_text(oui::translate("Updates", "The release lists no sum for {0}."), {program.asset}),
                     tag);
        return;
    }
    // Beside the one it replaces, so it is put in its place on the same disk.
    auto downloaded{program.executable};
    downloaded += ".update";
    const auto fetched{
        fetch_while_shown(application, doing, release_file_url(program, tag, program.asset), downloaded)};
    if(!fetched)
    {
        remove_quietly(downloaded);
        return;
    }
    const auto bytes{fetched->code == 0 ? read_file(downloaded) : std::nullopt};
    if(!bytes)
    {
        remove_quietly(downloaded);
        show_failure(application, program, cannot, fetched->error, tag);
        return;
    }
    if(sha256(*bytes) != *sum)
    {
        remove_quietly(downloaded);
        show_failure(application, program, cannot,
                     oui::translate("Updates", "What came is not what the release's sum says: it was left out."), tag);
        return;
    }
    if(const auto failed{replace_executable(program.executable, downloaded)})
    {
        remove_quietly(downloaded);
        show_failure(application, program, cannot, *failed, tag);
        return;
    }
    static_cast<void>(oui::message_box(
        application, oui::translate("Updates", "Check for Updates"),
        oui::format_text(oui::translate("Updates", "{0} {1} is installed: it is the one that starts the next time."),
                         {program.title, latest})));
}

// Says a newer version is out, and what updating to it takes where this one is.
void offer_update(oui::Application& application, const UpdateProgram& program, const std::string& tag)
{
    const auto title{oui::translate("Updates", "Check for Updates")};
    const auto out{oui::format_text(oui::translate("Updates", "{0} {1} is out; this one is {2}."),
                                    {program.title, version_of_tag(tag), program.version})};
    const auto page{[&] { static_cast<void>(oui::open_externally(release_page(program, tag))); }};
    const auto what_is_new{oui::translate("Updates", "What Is New")};
    switch(installation_of(program.executable, oui::path_from_utf8(environment("HOME")),
                           oui::path_from_utf8(environment("LOCALAPPDATA")), program_name, program_name))
    {
    case Installation::standalone:
    {
        const auto chosen{oui::message_box(
            application, title, out + "\n" + oui::translate("Updates", "Install it in place of this one?"),
            {oui::translate("Updates", "Install"), what_is_new, oui::translate("Updates", "Not Now")})};
        if(chosen == std::size_t{0})
        {
            install_update(application, program, tag);
        }
        else if(chosen == std::size_t{1})
        {
            page();
        }
        return;
    }
    case Installation::package:
    {
        const auto chosen{oui::message_box(
            application, title,
            out + "\n" + oui::translate("Updates", "APT installed it, and updates it with the rest of the system:") +
                "\nsudo apt update && sudo apt upgrade",
            {oui::translate("Updates", "OK"), what_is_new})};
        if(chosen == std::size_t{1})
        {
            page();
        }
        return;
    }
    case Installation::other:
    {
        const auto chosen{oui::message_box(
            application, title,
            out + "\n" +
                oui::translate("Updates", "This copy is not where the installers put it, so it is not replaced: the "
                                          "release's page has the new one."),
            {what_is_new, oui::translate("Updates", "OK")})};
        if(chosen == std::size_t{0})
        {
            page();
        }
        return;
    }
    }
}

} // namespace

UpdateProgram oui_paint_update_program()
{
#ifdef _WIN32
    const std::string system{"windows"};
#elif defined(__APPLE__)
    const std::string system{"macos"};
#else
    const std::string system{"linux"};
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    const std::string processor{system == "macos" ? "arm64" : "aarch64"};
#else
    const std::string processor{"x86_64"};
#endif
    UpdateProgram program;
    program.title = "OUI Paint";
    program.repository = "iganinja/oui_paint";
    // One kind, which opens in a window: "oui_paint-linux-x86_64".
    program.asset = std::string{program_name} + "-" + system + "-" + processor + (system == "windows" ? ".exe" : "");
    program.version = std::string{version()};
    program.executable = running_executable();
    return program;
}

std::optional<std::array<int, 3>> parse_version(std::string_view text)
{
    if(!text.empty() && text.front() == 'v')
    {
        text.remove_prefix(1);
    }
    std::array<int, 3> numbers{};
    std::size_t at{};
    for(std::size_t part{}; part < numbers.size(); ++ part)
    {
        if(part > 0)
        {
            if(at >= text.size() || text[at] != '.')
            {
                return std::nullopt;
            }
            ++ at;
        }
        const auto start{at};
        long long value{};
        while(at < text.size() && text[at] >= '0' && text[at] <= '9' && at - start < 9)
        {
            value = value * 10 + (text[at] - '0');
            ++ at;
        }
        if(at == start)
        {
            return std::nullopt;
        }
        numbers[part] = static_cast<int>(value);
    }
    // What a build adds after it, as git describe's "+3.ged0fda3", or a pre-release's "-rc1".
    if(at < text.size() && text[at] != '+' && text[at] != '-')
    {
        return std::nullopt;
    }
    return numbers;
}

bool is_newer(std::string_view latest, std::string_view current)
{
    const auto newest{parse_version(latest)};
    const auto running{parse_version(current)};
    return newest && running && *newest > *running;
}

std::optional<std::string> release_tag(std::string_view answer)
{
    constexpr std::string_view key{"\"tag_name\""};
    auto at{answer.find(key)};
    if(at == std::string_view::npos)
    {
        return std::nullopt;
    }
    at += key.size();
    const auto skip_space{[&]
                          {
                              while(at < answer.size() && (answer[at] == ' ' || answer[at] == '\t' ||
                                                           answer[at] == '\n' || answer[at] == '\r'))
                              {
                                  ++ at;
                              }
                          }};
    skip_space();
    if(at >= answer.size() || answer[at] != ':')
    {
        return std::nullopt;
    }
    ++ at;
    skip_space();
    if(at >= answer.size() || answer[at] != '"')
    {
        return std::nullopt;
    }
    const auto end{answer.find('"', at + 1)};
    if(end == std::string_view::npos || end == at + 1)
    {
        return std::nullopt;
    }
    return std::string{answer.substr(at + 1, end - at - 1)};
}

std::optional<std::string> listed_sum(std::string_view sums, std::string_view file)
{
    while(!sums.empty())
    {
        const auto end{sums.find('\n')};
        auto line{sums.substr(0, end)};
        sums = end == std::string_view::npos ? std::string_view{} : sums.substr(end + 1);
        if(!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        // "<sum>  <name>", or "<sum> *<name>" as a sum of bytes read in binary.
        const auto space{line.find(' ')};
        if(space != 64)
        {
            continue;
        }
        auto name{line.substr(space + 1)};
        if(!name.empty() && (name.front() == ' ' || name.front() == '*'))
        {
            name.remove_prefix(1);
        }
        if(name != file)
        {
            continue;
        }
        std::string sum;
        for(const auto digit : line.substr(0, space))
        {
            const auto lower{static_cast<char>(digit >= 'A' && digit <= 'F' ? digit - 'A' + 'a' : digit)};
            if(!((lower >= '0' && lower <= '9') || (lower >= 'a' && lower <= 'f')))
            {
                return std::nullopt;
            }
            sum.push_back(lower);
        }
        return sum;
    }
    return std::nullopt;
}

std::string sha256(std::string_view bytes)
{
    static constexpr std::uint32_t rounds[64]{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::uint32_t state[8]{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                           0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto rotate{[](std::uint32_t value, int by) { return (value >> by) | (value << (32 - by)); }};
    const auto block{
        [&](const unsigned char* data)
        {
            std::uint32_t words[64];
            for(int i{}; i < 16; ++ i)
            {
                words[i] = std::uint32_t{data[i * 4]} << 24 | std::uint32_t{data[i * 4 + 1]} << 16 |
                           std::uint32_t{data[i * 4 + 2]} << 8 | std::uint32_t{data[i * 4 + 3]};
            }
            for(int i{16}; i < 64; ++ i)
            {
                const auto s0{rotate(words[i - 15], 7) ^ rotate(words[i - 15], 18) ^ (words[i - 15] >> 3)};
                const auto s1{rotate(words[i - 2], 17) ^ rotate(words[i - 2], 19) ^ (words[i - 2] >> 10)};
                words[i] = words[i - 16] + s0 + words[i - 7] + s1;
            }
            auto a{state[0]};
            auto b{state[1]};
            auto c{state[2]};
            auto d{state[3]};
            auto e{state[4]};
            auto f{state[5]};
            auto g{state[6]};
            auto h{state[7]};
            for(int i{}; i < 64; ++ i)
            {
                const auto t1{h + (rotate(e, 6) ^ rotate(e, 11) ^ rotate(e, 25)) + ((e & f) ^ (~e & g)) + rounds[i] +
                              words[i]};
                const auto t2{(rotate(a, 2) ^ rotate(a, 13) ^ rotate(a, 22)) + ((a & b) ^ (a & c) ^ (b & c))};
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            state[0] += a;
            state[1] += b;
            state[2] += c;
            state[3] += d;
            state[4] += e;
            state[5] += f;
            state[6] += g;
            state[7] += h;
        }};
    const auto* const data{reinterpret_cast<const unsigned char*>(bytes.data())};
    std::size_t at{};
    for(; at + 64 <= bytes.size(); at += 64)
    {
        block(data + at);
    }
    // The rest, a 1 bit, zeros, and the length in bits, in one block or two.
    unsigned char last[128]{};
    const auto rest{bytes.size() - at};
    for(std::size_t i{}; i < rest; ++ i)
    {
        last[i] = data[at + i];
    }
    last[rest] = 0x80;
    const std::size_t blocks{rest < 56 ? 1u : 2u};
    const auto bits{static_cast<std::uint64_t>(bytes.size()) * 8};
    for(int i{}; i < 8; ++ i)
    {
        last[blocks * 64 - 1 - i] = static_cast<unsigned char>(bits >> (i * 8));
    }
    for(std::size_t i{}; i < blocks; ++ i)
    {
        block(last + i * 64);
    }
    static constexpr char digits[]{"0123456789abcdef"};
    std::string hex;
    for(const auto word : state)
    {
        for(int shift{28}; shift >= 0; shift -= 4)
        {
            hex.push_back(digits[(word >> shift) & 0xf]);
        }
    }
    return hex;
}

Installation installation_of(const std::filesystem::path& executable, const std::filesystem::path& home,
                             const std::filesystem::path& local_app_data, std::string_view name,
                             std::string_view folder)
{
    std::error_code ignored;
    const auto same{[&ignored](const std::filesystem::path& one, const std::filesystem::path& other)
                    {
                        return !one.empty() && !other.empty() &&
                               std::filesystem::weakly_canonical(one, ignored) ==
                                   std::filesystem::weakly_canonical(other, ignored);
                    }};
    const auto file{oui::utf8_of(executable.filename())};
#ifdef _WIN32
    static_cast<void>(home);
    if(file == std::string{name} + ".exe" && !local_app_data.empty() &&
       same(executable.parent_path(), local_app_data / "Programs" / oui::path_from_utf8(folder)))
    {
        return Installation::standalone;
    }
#else
    static_cast<void>(local_app_data);
    static_cast<void>(folder);
    if(same(executable, std::filesystem::path{"/usr/bin"} / oui::path_from_utf8(name)))
    {
        return Installation::package;
    }
    if(file == name && !home.empty() && same(executable.parent_path(), home / ".local" / "bin"))
    {
        return Installation::standalone;
    }
#endif
    return Installation::other;
}

std::filesystem::path running_executable()
{
    std::error_code ignored;
#ifdef _WIN32
    std::wstring path(32768, L'\0');
    const auto length{GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()))};
    if(length == 0 || length >= path.size())
    {
        return {};
    }
    path.resize(length);
    return std::filesystem::path{path};
#elif defined(__APPLE__)
    std::uint32_t size{0};
    static_cast<void>(_NSGetExecutablePath(nullptr, &size));
    std::string path(size, '\0');
    if(_NSGetExecutablePath(path.data(), &size) != 0)
    {
        return {};
    }
    path.resize(std::char_traits<char>::length(path.c_str()));
    return std::filesystem::weakly_canonical(std::filesystem::path{path}, ignored);
#else
    return std::filesystem::read_symlink("/proc/self/exe", ignored);
#endif
}

std::optional<std::vector<std::string>> fetch_command(std::string_view url, const std::filesystem::path& to,
                                                      std::string_view path)
{
#ifdef _WIN32
    const std::vector<std::string_view> tools{"curl", "powershell"};
#else
    const std::vector<std::string_view> tools{"curl", "wget"};
#endif
    const auto find{[path](std::string_view tool) -> std::optional<std::string>
                    {
                        auto rest{path};
                        while(true)
                        {
                            const auto end{rest.find(separator)};
                            const auto folder{rest.substr(0, end)};
                            if(!folder.empty())
                            {
                                for(const auto* const suffix : {"", ".exe"})
                                {
                                    const auto candidate{oui::path_from_utf8(folder) /
                                                         oui::path_from_utf8(std::string{tool} + suffix)};
                                    std::error_code ignored;
                                    if(std::filesystem::is_regular_file(candidate, ignored))
                                    {
                                        return oui::utf8_of(candidate);
                                    }
                                }
                            }
                            if(end == std::string_view::npos)
                            {
                                return std::nullopt;
                            }
                            rest = rest.substr(end + 1);
                        }
                    }};
    const auto target{oui::utf8_of(to)};
    for(const auto tool : tools)
    {
        const auto found{find(tool)};
        if(!found)
        {
            continue;
        }
        if(tool == "curl")
        {
            return std::vector<std::string>{*found, "-fsSL", "--connect-timeout", "30", "-o", target, std::string{url}};
        }
        if(tool == "wget")
        {
            return std::vector<std::string>{*found, "-q", "--timeout=30", "-O", target, std::string{url}};
        }
        // PowerShell takes a line of its own language, its strings in single quotes, doubled inside them.
        const auto quoted{[](std::string_view text)
                          {
                              std::string result{"'"};
                              for(const auto character : text)
                              {
                                  result += character == '\'' ? "''" : std::string(1, character);
                              }
                              return result + "'";
                          }};
        return std::vector<std::string>{*found, "-NoProfile", "-NonInteractive", "-Command",
                                        "$ProgressPreference = 'SilentlyContinue'; Invoke-WebRequest -UseBasicParsing "
                                        "-Uri " +
                                            quoted(url) + " -OutFile " + quoted(target)};
    }
    return std::nullopt;
}

std::optional<std::string> replace_executable(const std::filesystem::path& executable,
                                              const std::filesystem::path& downloaded)
{
    std::error_code error;
    const auto failed{[&error, &executable]
                      {
                          return oui::format_text(oui::translate("Updates", "It could not take the place of {0}: {1}"),
                                                  {oui::utf8_of(executable), error.message()});
                      }};
#ifdef _WIN32
    // Windows lets a program running be renamed, not written over.
    auto old{executable};
    old += ".old";
    std::filesystem::remove(old, error);
    std::filesystem::rename(executable, old, error);
    if(error)
    {
        return failed();
    }
    std::filesystem::rename(downloaded, executable, error);
    if(error)
    {
        std::error_code ignored;
        std::filesystem::rename(old, executable, ignored);
        return failed();
    }
#else
    using std::filesystem::perms;
    std::filesystem::permissions(
        downloaded, perms::owner_all | perms::group_read | perms::group_exec | perms::others_read | perms::others_exec,
        error);
    if(error)
    {
        return failed();
    }
    // The one running keeps its file while it runs; the name is the new one's from now on.
    std::filesystem::rename(downloaded, executable, error);
    if(error)
    {
        return failed();
    }
#endif
    return std::nullopt;
}

void remove_replaced(const std::filesystem::path& executable)
{
    if(executable.empty())
    {
        return;
    }
    auto old{executable};
    old += ".old";
    remove_quietly(old);
}

void check_for_updates(oui::Application& application, oui::Settings& settings, const UpdateProgram& program)
{
    const auto answer{temporary_file("latest")};
    const auto fetched{fetch_while_shown(
        application,
        oui::format_text(oui::translate("Updates", "Asking GitHub for the latest {0}..."), {program.title}),
        latest_url(program), answer)};
    const auto text{read_file(answer)};
    remove_quietly(answer);
    if(!fetched)
    {
        return;
    }
    const auto cannot{oui::format_text(oui::translate("Updates", "{0} could not ask GitHub for the latest version."),
                                       {program.title})};
    if(fetched->code != 0 || !text)
    {
        show_failure(application, program, cannot, fetched->error);
        return;
    }
    const auto tag{release_tag(*text)};
    if(!tag)
    {
        show_failure(application, program, cannot, oui::translate("Updates", "GitHub's answer names no release."));
        return;
    }
    remember_update_check(settings, seconds_now());
    if(!is_newer(*tag, program.version))
    {
        static_cast<void>(oui::message_box(application, oui::translate("Updates", "Check for Updates"),
                                           oui::format_text(oui::translate("Updates", "{0} {1} is the latest version."),
                                                            {program.title, program.version})));
        return;
    }
    offer_update(application, program, *tag);
}

bool update_check_due(const oui::Settings& settings, long long now)
{
    if(!settings.bool_value("check_for_updates", false))
    {
        return false;
    }
    const auto last{settings.value("updates_checked_at")};
    long long at{};
    for(const auto digit : last)
    {
        if(digit < '0' || digit > '9')
        {
            return true;
        }
        at = at * 10 + (digit - '0');
    }
    constexpr long long day{24 * 60 * 60};
    // A clock set back leaves the last check in the future: a check then sets it right.
    return last.empty() || now - at >= day || at > now;
}

void remember_update_check(oui::Settings& settings, long long now)
{
    settings.set_value("updates_checked_at", std::to_string(now));
    static_cast<void>(settings.save());
}

StartupUpdateCheck::StartupUpdateCheck(oui::Application& application, oui::Settings& settings, UpdateProgram program)
    : m_application{application}, m_settings{settings}, m_program{std::move(program)}
{
}

StartupUpdateCheck::~StartupUpdateCheck()
{
    m_process.reset();
    if(!m_answer.empty())
    {
        remove_quietly(m_answer);
    }
}

void StartupUpdateCheck::start(long long now, std::function<void(const std::string&)> found)
{
    if(m_process || !update_check_due(m_settings, now))
    {
        return;
    }
    m_answer = temporary_file("startup");
    const auto command{fetch_command(latest_url(m_program), m_answer, environment("PATH"))};
    if(!command)
    {
        return;
    }
    try
    {
        m_process = oui::start_process(m_application, {*command, {}, {}},
                                       {[](std::string_view) {}, [](std::string_view) {},
                                        [this, now, found = std::move(found)](int code)
                                        {
                                            const auto text{code == 0 ? read_file(m_answer) : std::nullopt};
                                            remove_quietly(m_answer);
                                            const auto tag{text ? release_tag(*text) : std::nullopt};
                                            // Said nothing when it failed: the next start tries again.
                                            if(!tag)
                                            {
                                                return;
                                            }
                                            remember_update_check(m_settings, now);
                                            if(!is_newer(*tag, m_program.version))
                                            {
                                                return;
                                            }
                                            if(found)
                                            {
                                                found(*tag);
                                                return;
                                            }
                                            // Offered once what tells it ended has returned, as the offer waits for an
                                            // answer.
                                            m_application.post([this, tag = *tag]
                                                               { offer_update(m_application, m_program, tag); });
                                        }});
    }
    catch(const std::system_error&)
    {
        m_process.reset();
    }
}

bool StartupUpdateCheck::is_running() const
{
    return m_process && m_process->is_running();
}

} // namespace oui_paint

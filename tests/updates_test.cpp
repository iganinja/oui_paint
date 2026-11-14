#include "app/updates.h"

#include <oui/application.h>
#include <oui/scripted_backend.h>
#include <oui/settings.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <thread>

using namespace olagarro;
using namespace oui_paint;

namespace
{

class UpdatesTests : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        folder =
            std::filesystem::temp_directory_path() /
            ("oui_paint_updates_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(folder);
    }

    void TearDown() override
    {
        std::error_code ignored;
        std::filesystem::remove_all(folder, ignored);
    }

    void write(const std::filesystem::path& path, const std::string& text)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream{path, std::ios::binary} << text;
    }

    [[nodiscard]] static std::string read(const std::filesystem::path& path)
    {
        std::ifstream file{path, std::ios::binary};
        return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    }

    std::filesystem::path folder;
};

} // namespace

TEST(Updates, VersionsAreThreeNumbersTheLaterOneNewer)
{
    EXPECT_EQ(parse_version("v0.4.7"), (std::array<int, 3>{0, 4, 7}));
    EXPECT_EQ(parse_version("0.4.7"), (std::array<int, 3>{0, 4, 7}));
    EXPECT_EQ(parse_version("0.4.0+3.ged0fda3"), (std::array<int, 3>{0, 4, 0})) << "as git describe gives it";
    EXPECT_FALSE(parse_version("0.4"));
    EXPECT_FALSE(parse_version("latest"));
    EXPECT_FALSE(parse_version("0.4.7x"));
    EXPECT_TRUE(is_newer("v0.4.10", "0.4.9")) << "numbers, not text";
    EXPECT_TRUE(is_newer("v1.0.0", "0.9.9"));
    EXPECT_FALSE(is_newer("v0.4.7", "0.4.7"));
    EXPECT_FALSE(is_newer("v0.4.6", "0.4.7"));
    EXPECT_FALSE(is_newer("nightly", "0.4.7"));
}

TEST(Updates, TheTagComesFromWhatGitHubSaysOfTheLatestRelease)
{
    EXPECT_EQ(release_tag(R"({"url": "x", "tag_name": "v0.4.8", "name": "v0.4.8"})"), std::string{"v0.4.8"});
    EXPECT_EQ(release_tag("{\n  \"tag_name\" :\n  \"v1.2.3\"\n}"), std::string{"v1.2.3"});
    EXPECT_FALSE(release_tag(R"({"message": "Not Found"})"));
    EXPECT_FALSE(release_tag(R"({"tag_name": ""})"));
}

TEST(Updates, TheSumOfAFileIsTheOneItsLineGives)
{
    const std::string a(64, 'a');
    const std::string b(64, 'B');
    const auto sums{a + "  oui_paint-linux-x86_64\n" + b + " *oui_paint-linux-aarch64\r\n"};
    EXPECT_EQ(listed_sum(sums, "oui_paint-linux-x86_64"), a);
    EXPECT_EQ(listed_sum(sums, "oui_paint-linux-aarch64"), std::string(64, 'b')) << "in lower case";
    EXPECT_FALSE(listed_sum(sums, "oui_paint-linux-x86_64.tar.gz"));
    EXPECT_FALSE(listed_sum("short  oui_paint\n", "oui_paint"));
}

TEST(Updates, Sha256IsTheStandardOne)
{
    EXPECT_EQ(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
              "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")
        << "two blocks of padding";
    EXPECT_EQ(sha256(std::string(1000000, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_F(UpdatesTests, WhereItIsSaysHowItWasInstalled)
{
    const auto home{folder / "home"};
#ifdef _WIN32
    const auto local{folder / "local"};
    write(local / "Programs" / "oui_paint" / "oui_paint.exe", "");
    EXPECT_EQ(installation_of(local / "Programs" / "oui_paint" / "oui_paint.exe", home, local, "oui_paint", "oui_paint"),
              Installation::standalone);
    EXPECT_EQ(installation_of(folder / "oui_paint.exe", home, local, "oui_paint", "oui_paint"), Installation::other);
#else
    write(home / ".local" / "bin" / "oui_paint", "");
    EXPECT_EQ(installation_of(home / ".local" / "bin" / "oui_paint", home, {}, "oui_paint", "oui_paint"),
              Installation::standalone);
    EXPECT_EQ(installation_of(home / ".local" / "bin" / "oui_paint-linux-x86_64", home, {}, "oui_paint", "oui_paint"),
              Installation::other)
        << "the release's file, not renamed as the installer renames it";
    EXPECT_EQ(installation_of("/usr/bin/oui_paint", home, {}, "oui_paint", "oui_paint"), Installation::package);
    EXPECT_EQ(installation_of(folder / "build" / "oui_paint", home, {}, "oui_paint", "oui_paint"), Installation::other);
    EXPECT_EQ(installation_of(home / ".local" / "bin" / "oui_paint", {}, {}, "oui_paint", "oui_paint"),
              Installation::other)
        << "with no home to say where";
#endif
}

TEST_F(UpdatesTests, ItDownloadsWithCurlThenWget)
{
#ifndef _WIN32
    const auto curl{folder / "one" / "curl"};
    const auto wget{folder / "two" / "wget"};
    write(curl, "");
    write(wget, "");
    const auto both{(folder / "one").string() + ":" + (folder / "two").string()};
    const auto with_curl{fetch_command("https://example.com/a", folder / "a", both)};
    ASSERT_TRUE(with_curl);
    EXPECT_EQ(with_curl->front(), curl.string());
    EXPECT_EQ(with_curl->back(), "https://example.com/a");
    const auto with_wget{fetch_command("https://example.com/a", folder / "a", (folder / "two").string())};
    ASSERT_TRUE(with_wget);
    EXPECT_EQ(with_wget->front(), wget.string());
    EXPECT_FALSE(fetch_command("https://example.com/a", folder / "a", (folder / "none").string()));
    EXPECT_FALSE(fetch_command("https://example.com/a", folder / "a", ""));
#endif
}

TEST_F(UpdatesTests, TheDownloadTakesThePlaceOfTheProgram)
{
    const auto executable{folder / "oui_paint"};
    const auto downloaded{folder / "oui_paint.update"};
    write(executable, "old");
    write(downloaded, "new");
    EXPECT_FALSE(replace_executable(executable, downloaded));
    EXPECT_EQ(read(executable), "new");
    EXPECT_FALSE(std::filesystem::exists(downloaded));
#ifndef _WIN32
    const auto permissions{std::filesystem::status(executable).permissions()};
    EXPECT_NE(permissions & std::filesystem::perms::owner_exec, std::filesystem::perms::none) << "runnable";
#endif
    EXPECT_TRUE(replace_executable(executable, folder / "missing")) << "says what failed";
    EXPECT_EQ(read(executable), "new") << "and leaves the program as it was";
    remove_replaced(executable);
    EXPECT_TRUE(std::filesystem::exists(executable));
}

TEST_F(UpdatesTests, TheCheckAsItStartsIsOnceADayWhenAsked)
{
    oui::Settings settings{folder / "settings.ini"};
    EXPECT_FALSE(update_check_due(settings, 1'000'000)) << "only when Preferences ask";
    settings.set_bool_value("check_for_updates", true);
    EXPECT_TRUE(update_check_due(settings, 1'000'000));
    remember_update_check(settings, 1'000'000);
    EXPECT_FALSE(update_check_due(settings, 1'000'000 + 60 * 60));
    EXPECT_TRUE(update_check_due(settings, 1'000'000 + 24 * 60 * 60));
    EXPECT_TRUE(update_check_due(settings, 999'000)) << "a clock set back";
}

#ifndef _WIN32
TEST_F(UpdatesTests, AsItStartsItHearsOfANewerVersionFromGitHub)
{
    // A curl of its own, which writes what GitHub would say where it is asked to.
    const auto tools{folder / "tools"};
    write(tools / "curl", "#!/bin/sh\n"
                          "while [ \"$1\" != -o ]; do shift; done\n"
                          "printf '{\"tag_name\": \"v9.0.0\"}' > \"$2\"\n");
    std::filesystem::permissions(tools / "curl", std::filesystem::perms::owner_all);
    const auto* const saved{std::getenv("PATH")};
    const std::string kept{saved != nullptr ? saved : ""};
    setenv("PATH", (tools.string() + ":" + kept).c_str(), 1);
    oui::Application application{oui::scripted_backend()};
    oui::Settings settings{folder / "settings.ini"};
    settings.set_bool_value("check_for_updates", true);
    UpdateProgram program;
    program.title = "OUI Paint";
    program.repository = "iganinja/oui_paint";
    program.version = "0.1.6";
    std::optional<std::string> found;
    StartupUpdateCheck check{application, settings, program};
    check.start(1'000'000, [&found](const std::string& tag) { found = tag; });
    const auto deadline{std::chrono::steady_clock::now() + std::chrono::seconds{5}};
    while(!found && std::chrono::steady_clock::now() < deadline)
    {
        application.run_frame();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    setenv("PATH", kept.c_str(), 1);
    EXPECT_EQ(found, std::string{"v9.0.0"});
    EXPECT_FALSE(update_check_due(settings, 1'000'000 + 60)) << "the check is remembered";
}
#endif

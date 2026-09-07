#include "check.h"

#include "core/Settings.h"

#include <filesystem>
#include <fstream>

using namespace citron;

namespace {

std::filesystem::path tempDir() {
    auto dir = std::filesystem::temp_directory_path() / "citron-tests";
    std::filesystem::create_directories(dir);
    return dir;
}

}

TEST_CASE(settings_round_trip) {
    Settings s;
    s.language = "de";
    s.theme = "light";
    s.closeOnLaunch = false;
    s.keepInstallers = false;
    s.selectedVersion = "release/1.26.45.01";
    s.windowWidth = 1100;
    s.windowHeight = 660;
    s.windowMaximized = true;
    s.lastUpdateCheck = 1700000000;
    Settings back = settingsFromJson(settingsToJson(s));
    CHECK(back == s);
}

TEST_CASE(settings_defaults_for_bad_values) {
    auto v = json::parse(R"({"language":"fr","theme":"blue","windowWidth":5,"closeOnLaunch":"yes"})");
    CHECK(v.has_value());
    Settings s = settingsFromJson(*v);
    CHECK_EQ(s.language, "en");
    CHECK_EQ(s.theme, "dark");
    CHECK_EQ(s.windowWidth, 200);
    CHECK(s.closeOnLaunch);
}

TEST_CASE(settings_save_and_load) {
    const auto file = tempDir() / "settings.json";
    Settings s;
    s.selectedVersion = "preview/1.26.60.21";
    s.keepInstallers = false;
    auto saved = saveSettings(s, file);
    CHECK(saved.has_value());
    CHECK(!std::filesystem::exists(file.wstring() + L".tmp"));
    auto loaded = loadSettings(file);
    CHECK(!loaded.recovered);
    CHECK(loaded.settings == s);
    std::filesystem::remove(file);
}

TEST_CASE(settings_recovers_from_corruption) {
    const auto file = tempDir() / "settings-corrupt.json";
    {
        std::ofstream out(file, std::ios::binary);
        out << "{\"language\": \"de\", ";
    }
    auto loaded = loadSettings(file);
    CHECK(loaded.recovered);
    CHECK(loaded.settings == Settings{});
    std::filesystem::remove(file);
    auto missing = loadSettings(tempDir() / "does-not-exist.json");
    CHECK(!missing.recovered);
    CHECK(missing.settings == Settings{});
}

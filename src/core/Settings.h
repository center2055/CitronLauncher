#pragma once

#include "core/Error.h"
#include "core/Json.h"

#include <filesystem>
#include <string>

namespace citron {

struct Settings {
    std::string language = "en";
    std::string theme = "dark";
    bool closeOnLaunch = true;
    bool keepInstallers = true;
    bool checkUpdates = true;
    std::string selectedVersion;
    std::string rootDirectory;
    std::string catalogUrl;
    int windowWidth = 880;
    int windowHeight = 540;
    bool windowMaximized = false;
    std::int64_t lastUpdateCheck = 0;
    bool verboseLogging = false;

    bool operator==(const Settings&) const = default;
};

json::Value settingsToJson(const Settings& settings);
Settings settingsFromJson(const json::Value& value);

struct LoadedSettings {
    Settings settings;
    bool recovered = false;
};

LoadedSettings loadSettings(const std::filesystem::path& file);
Result<void> saveSettings(const Settings& settings, const std::filesystem::path& file);
Result<void> writeFileAtomically(const std::filesystem::path& file, std::string_view content);
Result<std::string> readFile(const std::filesystem::path& file);

}

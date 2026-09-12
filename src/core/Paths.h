#pragma once

#include "core/Error.h"

#include <filesystem>

namespace citron::paths {

struct Layout {
    std::filesystem::path root;
    std::filesystem::path settingsFile;
    std::filesystem::path installers;
    std::filesystem::path downloads;
    std::filesystem::path versions;
    std::filesystem::path cache;
    std::filesystem::path logs;
};

std::filesystem::path localAppData();
std::filesystem::path roamingAppData();
Layout layoutFor(const std::filesystem::path& root);
Layout defaultLayout();
Result<void> ensureDirectories(const Layout& layout);
std::filesystem::path executablePath();

}

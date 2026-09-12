#pragma once

#include "core/Error.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace citron::platform {

struct InstalledPackage {
    std::wstring fullName;
    std::wstring familyName;
    std::wstring name;
    std::wstring version;
    std::filesystem::path installLocation;
    bool developmentMode = false;
    bool isFramework = false;
};

std::vector<InstalledPackage> findPackagesByFamily(std::wstring_view familyName);
std::vector<InstalledPackage> findPackagesByName(std::wstring_view name);
std::optional<InstalledPackage> findPackageByName(std::wstring_view name);
Result<void> removePackage(std::wstring_view fullName, std::stop_token token);
Result<void> launchPackage(std::wstring_view familyName);
void initializeApartment();

}

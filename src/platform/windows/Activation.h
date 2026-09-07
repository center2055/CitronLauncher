#pragma once

#include "core/Error.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace citron::platform {

struct ActivatedProcess {
    unsigned long pid = 0;
    void* handle = nullptr;
};

Result<ActivatedProcess> activatePackageExecutable(std::wstring_view appUserModelId, std::wstring_view executable, const std::filesystem::path& workingDirectory);
std::optional<std::wstring> appUserModelId(std::wstring_view familyName);
std::wstring gameExecutable(const std::filesystem::path& installLocation);

}

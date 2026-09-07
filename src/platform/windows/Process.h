#pragma once

#include <windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace citron::platform {

struct ProcessInfo {
    DWORD pid = 0;
    std::filesystem::path path;
};

std::vector<ProcessInfo> findProcesses(std::wstring_view exeName);
bool isProcessAlive(DWORD pid);
bool anyProcessUnder(std::wstring_view exeName, const std::filesystem::path& directory);

}

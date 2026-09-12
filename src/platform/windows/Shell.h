#pragma once

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string_view>

namespace citron::platform {

bool openUrl(std::wstring_view url);
std::optional<std::filesystem::path> pickFolder(HWND owner, const std::filesystem::path& initial);
bool copyToClipboard(HWND owner, std::wstring_view text);
std::optional<std::wstring> readClipboard(HWND owner);

}

#pragma once

#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>

namespace citron::platform {

bool registryKeyExists(HKEY root, const wchar_t* subkey);
std::optional<std::uint32_t> registryReadDword(HKEY root, const wchar_t* subkey, const wchar_t* value);
std::optional<std::wstring> registryReadString(HKEY root, const wchar_t* subkey, const wchar_t* value);

}

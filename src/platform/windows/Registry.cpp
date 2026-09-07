#include "platform/windows/Registry.h"

#include <vector>

namespace citron::platform {

namespace {

struct KeyHandle {
    HKEY key = nullptr;
    ~KeyHandle() {
        if (key != nullptr) {
            RegCloseKey(key);
        }
    }
};

bool openRead(HKEY root, const wchar_t* subkey, KeyHandle& out) {
    return RegOpenKeyExW(root, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &out.key) == ERROR_SUCCESS;
}

}

bool registryKeyExists(HKEY root, const wchar_t* subkey) {
    KeyHandle key;
    return openRead(root, subkey, key);
}

std::optional<std::uint32_t> registryReadDword(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    KeyHandle key;
    if (!openRead(root, subkey, key)) {
        return std::nullopt;
    }
    DWORD data = 0;
    DWORD size = sizeof(data);
    DWORD type = 0;
    if (RegQueryValueExW(key.key, value, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size) != ERROR_SUCCESS || type != REG_DWORD) {
        return std::nullopt;
    }
    return data;
}

std::optional<std::wstring> registryReadString(HKEY root, const wchar_t* subkey, const wchar_t* value) {
    KeyHandle key;
    if (!openRead(root, subkey, key)) {
        return std::nullopt;
    }
    DWORD size = 0;
    DWORD type = 0;
    if (RegQueryValueExW(key.key, value, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
        return std::nullopt;
    }
    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key.key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    return std::wstring(buffer.data());
}

}

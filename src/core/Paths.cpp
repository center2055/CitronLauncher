#include "core/Paths.h"

#include <windows.h>
#include <shlobj.h>

#include <vector>

namespace citron::paths {

namespace {

std::filesystem::path knownFolder(const KNOWNFOLDERID& id) {
    PWSTR raw = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw)) && raw != nullptr) {
        result = raw;
    }
    if (raw != nullptr) {
        CoTaskMemFree(raw);
    }
    return result;
}

}

std::filesystem::path localAppData() {
    return knownFolder(FOLDERID_LocalAppData);
}

std::filesystem::path roamingAppData() {
    return knownFolder(FOLDERID_RoamingAppData);
}

Layout layoutFor(const std::filesystem::path& root) {
    Layout layout;
    layout.root = root;
    layout.settingsFile = root / L"settings.json";
    layout.installers = root / L"installers";
    layout.downloads = root / L"downloads";
    layout.cache = root / L"cache";
    layout.logs = root / L"logs";
    return layout;
}

Layout defaultLayout() {
    return layoutFor(localAppData() / L"CitronLauncher");
}

Result<void> ensureDirectories(const Layout& layout) {
    for (const auto* dir : {&layout.root, &layout.installers, &layout.downloads, &layout.cache, &layout.logs}) {
        std::error_code ec;
        std::filesystem::create_directories(*dir, ec);
        if (ec) {
            return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "create directories", static_cast<unsigned long>(ec.value()),
                                                    "The launcher folder could not be created: " + dir->string()));
        }
    }
    return {};
}

std::filesystem::path executablePath() {
    std::vector<wchar_t> buffer(MAX_PATH);
    while (true) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(buffer.data(), buffer.data() + length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

}

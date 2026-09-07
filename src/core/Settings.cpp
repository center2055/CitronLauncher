#include "core/Settings.h"

#include "core/Logger.h"

#include <windows.h>

#include <algorithm>

namespace citron {

json::Value settingsToJson(const Settings& s) {
    json::Value v = json::Object{};
    v.set("language", s.language);
    v.set("theme", s.theme);
    v.set("closeOnLaunch", s.closeOnLaunch);
    v.set("keepInstallers", s.keepInstallers);
    v.set("checkUpdates", s.checkUpdates);
    v.set("selectedVersion", s.selectedVersion);
    v.set("rootDirectory", s.rootDirectory);
    v.set("catalogUrl", s.catalogUrl);
    v.set("windowWidth", s.windowWidth);
    v.set("windowHeight", s.windowHeight);
    v.set("windowMaximized", s.windowMaximized);
    v.set("lastUpdateCheck", s.lastUpdateCheck);
    v.set("verboseLogging", s.verboseLogging);
    return v;
}

Settings settingsFromJson(const json::Value& v) {
    Settings d;
    Settings s;
    s.language = v["language"].asString(d.language);
    if (s.language != "en" && s.language != "de") {
        s.language = d.language;
    }
    s.theme = v["theme"].asString(d.theme);
    if (s.theme != "dark" && s.theme != "light") {
        s.theme = d.theme;
    }
    s.closeOnLaunch = v["closeOnLaunch"].asBool(d.closeOnLaunch);
    s.keepInstallers = v["keepInstallers"].asBool(d.keepInstallers);
    s.checkUpdates = v["checkUpdates"].asBool(d.checkUpdates);
    s.selectedVersion = v["selectedVersion"].asString();
    s.rootDirectory = v["rootDirectory"].asString();
    s.catalogUrl = v["catalogUrl"].asString();
    s.windowWidth = std::clamp(static_cast<int>(v["windowWidth"].asInt(d.windowWidth)), 200, 10000);
    s.windowHeight = std::clamp(static_cast<int>(v["windowHeight"].asInt(d.windowHeight)), 150, 10000);
    s.windowMaximized = v["windowMaximized"].asBool(false);
    s.lastUpdateCheck = v["lastUpdateCheck"].asInt(0);
    s.verboseLogging = v["verboseLogging"].asBool(false);
    return s;
}

Result<std::string> readFile(const std::filesystem::path& file) {
    HANDLE h = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "read file", GetLastError(), "The file could not be opened: " + file.string()));
    }
    std::string content;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(h, &size) && size.QuadPart > 0 && size.QuadPart < (1ll << 31)) {
        content.resize(static_cast<size_t>(size.QuadPart));
        size_t offset = 0;
        while (offset < content.size()) {
            DWORD read = 0;
            const DWORD chunk = static_cast<DWORD>(std::min<size_t>(content.size() - offset, 1 << 20));
            if (!ReadFile(h, content.data() + offset, chunk, &read, nullptr)) {
                const DWORD err = GetLastError();
                CloseHandle(h);
                return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "read file", err, "The file could not be read: " + file.string()));
            }
            if (read == 0) {
                content.resize(offset);
                break;
            }
            offset += read;
        }
    }
    CloseHandle(h);
    return content;
}

Result<void> writeFileAtomically(const std::filesystem::path& file, std::string_view content) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    const std::filesystem::path temp = file.wstring() + L".tmp";
    HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "write file", GetLastError(), "The file could not be created: " + temp.string()));
    }
    size_t offset = 0;
    while (offset < content.size()) {
        DWORD written = 0;
        const DWORD chunk = static_cast<DWORD>(std::min<size_t>(content.size() - offset, 1 << 20));
        if (!WriteFile(h, content.data() + offset, chunk, &written, nullptr)) {
            const DWORD err = GetLastError();
            CloseHandle(h);
            DeleteFileW(temp.c_str());
            return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "write file", err, "The file could not be written: " + temp.string()));
        }
        offset += written;
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD err = GetLastError();
        DeleteFileW(temp.c_str());
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "replace file", err, "The file could not be replaced: " + file.string()));
    }
    return {};
}

LoadedSettings loadSettings(const std::filesystem::path& file) {
    LoadedSettings out;
    auto content = readFile(file);
    if (!content) {
        if (content.error().code != static_cast<std::uint32_t>(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) &&
            content.error().code != static_cast<std::uint32_t>(HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND))) {
            log::warn("settings: {}", content.error().summary());
            out.recovered = true;
        }
        return out;
    }
    auto parsed = json::parse(*content);
    if (!parsed || !parsed->isObject()) {
        log::warn("settings: invalid file, using defaults ({})", parsed ? "not an object" : parsed.error());
        out.recovered = true;
        return out;
    }
    out.settings = settingsFromJson(*parsed);
    return out;
}

Result<void> saveSettings(const Settings& settings, const std::filesystem::path& file) {
    return writeFileAtomically(file, json::serialize(settingsToJson(settings)));
}

}

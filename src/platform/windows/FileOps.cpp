#include "platform/windows/FileOps.h"

#include <windows.h>

#include <chrono>

namespace citron::platform {

std::optional<std::uint64_t> fileSize(const std::filesystem::path& file) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(file.c_str(), GetFileExInfoStandard, &data)) {
        return std::nullopt;
    }
    if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        return std::nullopt;
    }
    return (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}

bool fileExists(const std::filesystem::path& file) {
    return fileSize(file).has_value();
}

Result<void> moveReplace(const std::filesystem::path& from, const std::filesystem::path& to) {
    if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "move file", GetLastError(), "The file could not be moved into place: " + to.string()));
    }
    return {};
}

Result<void> removeFile(const std::filesystem::path& file) {
    if (!DeleteFileW(file.c_str())) {
        const DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            return {};
        }
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "delete file", err, "The file could not be deleted: " + file.string()));
    }
    return {};
}

void discardFile(const std::filesystem::path& file) {
    DeleteFileW(file.c_str());
}

std::optional<std::uint64_t> freeSpace(const std::filesystem::path& directory) {
    ULARGE_INTEGER available{};
    if (!GetDiskFreeSpaceExW(directory.c_str(), &available, nullptr, nullptr)) {
        return std::nullopt;
    }
    return available.QuadPart;
}

std::int64_t unixNow() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool isWritableDirectory(const std::filesystem::path& directory) {
    std::error_code ec;
    if (!std::filesystem::is_directory(directory, ec)) {
        return false;
    }
    const std::filesystem::path probe = directory / L".citron-write-test";
    HANDLE h = CreateFileW(probe.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        return false;
    }
    CloseHandle(h);
    return true;
}

}

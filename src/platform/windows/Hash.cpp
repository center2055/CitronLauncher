#include "platform/windows/Hash.h"

#include <windows.h>
#include <bcrypt.h>

#include <format>
#include <vector>

namespace citron::platform {

namespace {

struct AlgorithmHandle {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~AlgorithmHandle() {
        if (handle != nullptr) {
            BCryptCloseAlgorithmProvider(handle, 0);
        }
    }
};

struct HashHandle {
    BCRYPT_HASH_HANDLE handle = nullptr;
    ~HashHandle() {
        if (handle != nullptr) {
            BCryptDestroyHash(handle);
        }
    }
};

struct FileHandle {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~FileHandle() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

Error hashError(const char* what, NTSTATUS status) {
    return Error::make(ErrorCategory::Internal, "hash", "The download could not be verified.", std::format("{} failed with status 0x{:08X}", what, static_cast<unsigned long>(status)));
}

}

Result<std::string> md5OfFile(const std::filesystem::path& file, std::stop_token token, const HashProgress& progress) {
    AlgorithmHandle alg;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&alg.handle, BCRYPT_MD5_ALGORITHM, nullptr, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return std::unexpected(hashError("BCryptOpenAlgorithmProvider", status));
    }
    DWORD hashLength = 0;
    DWORD resultLength = 0;
    status = BCryptGetProperty(alg.handle, BCRYPT_HASH_LENGTH, reinterpret_cast<PUCHAR>(&hashLength), sizeof(hashLength), &resultLength, 0);
    if (!BCRYPT_SUCCESS(status) || hashLength == 0) {
        return std::unexpected(hashError("BCryptGetProperty", status));
    }
    HashHandle hash;
    status = BCryptCreateHash(alg.handle, &hash.handle, nullptr, 0, nullptr, 0, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return std::unexpected(hashError("BCryptCreateHash", status));
    }

    FileHandle in;
    in.handle = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (in.handle == INVALID_HANDLE_VALUE) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "hash", GetLastError(), "The downloaded file could not be opened for verification."));
    }
    LARGE_INTEGER size{};
    GetFileSizeEx(in.handle, &size);
    const auto total = static_cast<std::uint64_t>(size.QuadPart);

    std::vector<unsigned char> buffer(4 * 1024 * 1024);
    std::uint64_t done = 0;
    std::uint64_t lastReport = 0;
    while (true) {
        if (token.stop_requested()) {
            return std::unexpected(Error::cancelled("hash"));
        }
        DWORD read = 0;
        if (!ReadFile(in.handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr)) {
            return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "hash", GetLastError(), "The downloaded file could not be read for verification."));
        }
        if (read == 0) {
            break;
        }
        status = BCryptHashData(hash.handle, buffer.data(), read, 0);
        if (!BCRYPT_SUCCESS(status)) {
            return std::unexpected(hashError("BCryptHashData", status));
        }
        done += read;
        if (progress && done - lastReport >= 32 * 1024 * 1024) {
            lastReport = done;
            progress(done, total);
        }
    }
    std::vector<unsigned char> digest(hashLength);
    status = BCryptFinishHash(hash.handle, digest.data(), hashLength, 0);
    if (!BCRYPT_SUCCESS(status)) {
        return std::unexpected(hashError("BCryptFinishHash", status));
    }
    if (progress) {
        progress(total, total);
    }
    std::string hex;
    hex.reserve(hashLength * 2);
    for (const unsigned char b : digest) {
        hex += std::format("{:02x}", b);
    }
    return hex;
}

}

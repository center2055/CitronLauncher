#include "core/Error.h"

#include "core/Text.h"

#include <windows.h>

#include <format>

namespace citron {

Error Error::make(ErrorCategory category, std::string operation, std::string message, std::string detail, bool retryable) {
    Error e;
    e.category = category;
    e.operation = std::move(operation);
    e.message = std::move(message);
    e.detail = std::move(detail);
    e.retryable = retryable;
    return e;
}

Error Error::fromHresult(ErrorCategory category, std::string operation, long hresult, std::string message) {
    Error e = make(category, std::move(operation), std::move(message));
    e.code = static_cast<std::uint32_t>(hresult);
    e.detail = systemMessage(e.code);
    return e;
}

Error Error::fromWin32(ErrorCategory category, std::string operation, unsigned long win32, std::string message) {
    Error e = make(category, std::move(operation), std::move(message));
    e.code = static_cast<std::uint32_t>(HRESULT_FROM_WIN32(win32));
    e.detail = systemMessage(win32);
    return e;
}

Error Error::cancelled(std::string operation) {
    return make(ErrorCategory::Cancelled, std::move(operation), "Cancelled");
}

std::string Error::codeText() const {
    if (code == 0) {
        return {};
    }
    return std::format("0x{:08X}", code);
}

std::string Error::summary() const {
    std::string out = message;
    if (!detail.empty()) {
        out += " (" + detail + ")";
    }
    if (code != 0) {
        out += " " + codeText();
    }
    return out;
}

std::string systemMessage(unsigned long code) {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags, nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                        reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::string out;
    if (length > 0 && buffer != nullptr) {
        out = text::trim(text::toUtf8(std::wstring_view(buffer, length)));
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return out;
}

std::string_view categoryName(ErrorCategory category) {
    switch (category) {
    case ErrorCategory::Network: return "network";
    case ErrorCategory::Download: return "download";
    case ErrorCategory::VersionMetadata: return "metadata";
    case ErrorCategory::Filesystem: return "filesystem";
    case ErrorCategory::Package: return "package";
    case ErrorCategory::Gdk: return "gdk";
    case ErrorCategory::Prerequisite: return "prerequisite";
    case ErrorCategory::Launch: return "launch";
    case ErrorCategory::Configuration: return "configuration";
    case ErrorCategory::Cancelled: return "cancelled";
    case ErrorCategory::Internal: return "internal";
    }
    return "unknown";
}

}

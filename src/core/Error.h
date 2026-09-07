#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace citron {

enum class ErrorCategory {
    Network,
    Download,
    VersionMetadata,
    Filesystem,
    Package,
    Gdk,
    Prerequisite,
    Launch,
    Configuration,
    Cancelled,
    Internal,
};

struct Error {
    ErrorCategory category = ErrorCategory::Internal;
    std::string operation;
    std::string message;
    std::string detail;
    std::uint32_t code = 0;
    bool retryable = false;

    static Error make(ErrorCategory category, std::string operation, std::string message, std::string detail = {}, bool retryable = false);
    static Error fromHresult(ErrorCategory category, std::string operation, long hresult, std::string message);
    static Error fromWin32(ErrorCategory category, std::string operation, unsigned long win32, std::string message);
    static Error cancelled(std::string operation);

    bool isCancelled() const { return category == ErrorCategory::Cancelled; }
    std::string codeText() const;
    std::string summary() const;
};

std::string systemMessage(unsigned long code);
std::string_view categoryName(ErrorCategory category);

template <typename T>
using Result = std::expected<T, Error>;

}

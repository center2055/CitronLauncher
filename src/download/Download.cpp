#include "download/Download.h"

#include "core/Logger.h"
#include "core/Text.h"
#include "download/DownloadMeta.h"
#include "download/Http.h"
#include "minecraft/InstallState.h"
#include "platform/windows/FileOps.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <format>
#include <mutex>
#include <thread>
#include <vector>

namespace citron {

namespace {

constexpr int kMaxAttempts = 6;
constexpr size_t kChunk = 256 * 1024;

class CancellableRequest {
public:
    CancellableRequest(HINTERNET request, std::stop_token token)
        : request_(request), callback_(token, [this] { abort(); }) {}

    ~CancellableRequest() {
        std::lock_guard lock(mutex_);
        if (request_ != nullptr) {
            WinHttpCloseHandle(request_);
            request_ = nullptr;
        }
    }

    HINTERNET get() const { return request_; }

private:
    void abort() {
        std::lock_guard lock(mutex_);
        if (request_ != nullptr) {
            WinHttpCloseHandle(request_);
            request_ = nullptr;
        }
    }

    std::mutex mutex_;
    HINTERNET request_;
    std::stop_callback<std::function<void()>> callback_;
};

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h != nullptr) {
            WinHttpCloseHandle(h);
        }
    }
};

struct FileHandle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~FileHandle() {
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
        }
    }
};

class SpeedMeter {
public:
    void add(std::uint64_t bytes) {
        const auto now = std::chrono::steady_clock::now();
        samples_.push_back({now, bytes});
        while (!samples_.empty() && now - samples_.front().time > std::chrono::seconds(3)) {
            samples_.pop_front();
        }
    }

    double bytesPerSecond() const {
        if (samples_.size() < 2) {
            return 0.0;
        }
        std::uint64_t bytes = 0;
        for (size_t i = 1; i < samples_.size(); ++i) {
            bytes += samples_[i].bytes;
        }
        const double seconds = std::chrono::duration<double>(samples_.back().time - samples_.front().time).count();
        return seconds > 0.0 ? static_cast<double>(bytes) / seconds : 0.0;
    }

private:
    struct Sample {
        std::chrono::steady_clock::time_point time;
        std::uint64_t bytes;
    };
    std::deque<Sample> samples_;
};

std::wstring queryHeader(HINTERNET request, DWORD info) {
    DWORD size = 0;
    WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return {};
    }
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (!WinHttpQueryHeaders(request, info, WINHTTP_HEADER_NAME_BY_INDEX, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
        return {};
    }
    value.resize(size / sizeof(wchar_t));
    return value;
}

std::optional<std::uint64_t> parseContentRangeTotal(const std::wstring& header) {
    const auto slash = header.rfind(L'/');
    if (slash == std::wstring::npos) {
        return std::nullopt;
    }
    const std::wstring total = header.substr(slash + 1);
    if (total.empty() || total == L"*") {
        return std::nullopt;
    }
    try {
        return std::stoull(total);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> parseContentRangeStart(const std::wstring& header) {
    const auto space = header.find(L' ');
    const auto dash = header.find(L'-');
    if (space == std::wstring::npos || dash == std::wstring::npos || dash <= space) {
        return std::nullopt;
    }
    try {
        return std::stoull(header.substr(space + 1, dash - space - 1));
    } catch (...) {
        return std::nullopt;
    }
}

Error networkError(DWORD code, std::string message) {
    Error e = Error::fromWin32(ErrorCategory::Download, "download", code, std::move(message));
    e.retryable = true;
    return e;
}

struct Attempt {
    bool retry = false;
    Error error;
};

Attempt fail(Error error) {
    Attempt a;
    a.retry = error.retryable;
    a.error = std::move(error);
    return a;
}

std::expected<void, Attempt> runAttempt(const DownloadRequest& request, const std::string& url, std::stop_token token, const DownloadProgressSink& progress) {
    const auto parsed = http::parseUrl(url);
    if (!parsed) {
        return std::unexpected(fail(Error::make(ErrorCategory::Download, "download", "The download address is not valid.", url)));
    }

    std::uint64_t offset = 0;
    if (const auto existing = platform::fileSize(request.partialFile)) {
        PartialFile partial;
        partial.size = *existing;
        if (const auto meta = readDownloadMeta(request.partialFile)) {
            partial.url = meta->url;
            partial.etag = meta->etag;
            partial.lastModified = meta->lastModified;
            partial.totalSize = meta->totalSize;
            partial.md5 = meta->md5;
        }
        if (decideResume(partial, url, request.expectedSize, request.md5) == ResumeDecision::Resume) {
            offset = *existing;
        }
    }

    Handle session;
    session.h = WinHttpOpen(http::userAgent(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session.h == nullptr) {
        return std::unexpected(fail(networkError(GetLastError(), "The network session could not be opened.")));
    }
    WinHttpSetTimeouts(session.h, 10000, 15000, 30000, 30000);
    Handle connection;
    connection.h = WinHttpConnect(session.h, parsed->host.c_str(), parsed->port, 0);
    if (connection.h == nullptr) {
        return std::unexpected(fail(networkError(GetLastError(), "The download server could not be reached.")));
    }
    HINTERNET raw = WinHttpOpenRequest(connection.h, L"GET", parsed->path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       parsed->secure ? WINHTTP_FLAG_SECURE : 0);
    if (raw == nullptr) {
        return std::unexpected(fail(networkError(GetLastError(), "The download request could not be created.")));
    }
    CancellableRequest req(raw, token);

    std::wstring headers;
    if (offset > 0) {
        headers = std::format(L"Range: bytes={}-\r\n", offset);
        log::info("resuming download at {} bytes", offset);
    }
    if (!WinHttpSendRequest(req.get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), headers.empty() ? 0 : static_cast<DWORD>(-1),
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.get(), nullptr)) {
        if (token.stop_requested()) {
            return std::unexpected(fail(Error::cancelled("download")));
        }
        return std::unexpected(fail(networkError(GetLastError(), "The download server did not respond.")));
    }

    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(req.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);

    if (status == 416) {
        platform::discardFile(request.partialFile);
        removeDownloadMeta(request.partialFile);
        Error e = Error::make(ErrorCategory::Download, "download", "The partial download could not be resumed.", url, true);
        return std::unexpected(fail(e));
    }
    if (status != 200 && status != 206) {
        Error e = Error::make(ErrorCategory::Download, "download", std::format("The download server answered with status {}.", status), url, status >= 500 || status == 429);
        return std::unexpected(fail(e));
    }

    std::uint64_t total = 0;
    if (status == 206) {
        const std::wstring range = queryHeader(req.get(), WINHTTP_QUERY_CONTENT_RANGE);
        const auto start = parseContentRangeStart(range);
        if (!start || *start != offset) {
            platform::discardFile(request.partialFile);
            removeDownloadMeta(request.partialFile);
            return std::unexpected(fail(Error::make(ErrorCategory::Download, "download", "The partial download could not be resumed.", url, true)));
        }
        total = parseContentRangeTotal(range).value_or(0);
    } else {
        offset = 0;
        const std::wstring length = queryHeader(req.get(), WINHTTP_QUERY_CONTENT_LENGTH);
        try {
            total = length.empty() ? 0 : std::stoull(length);
        } catch (...) {
            total = 0;
        }
    }
    if (request.expectedSize != 0 && total != 0 && total != request.expectedSize) {
        Error e = Error::make(ErrorCategory::Download, "download", "The package on the server has a different size than expected.",
                              std::format("expected {} bytes, server reports {}", request.expectedSize, total));
        return std::unexpected(fail(e));
    }
    if (total == 0) {
        total = request.expectedSize;
    }

    DownloadMeta meta;
    meta.url = url;
    meta.etag = text::toUtf8(queryHeader(req.get(), WINHTTP_QUERY_ETAG));
    meta.lastModified = text::toUtf8(queryHeader(req.get(), WINHTTP_QUERY_LAST_MODIFIED));
    meta.totalSize = total;
    meta.md5 = request.md5;
    meta.started = platform::unixNow();
    if (auto written = writeDownloadMeta(request.partialFile, meta); !written) {
        return std::unexpected(fail(written.error()));
    }

    FileHandle file;
    file.h = CreateFileW(request.partialFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, offset > 0 ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.h == INVALID_HANDLE_VALUE) {
        return std::unexpected(fail(Error::fromWin32(ErrorCategory::Filesystem, "download", GetLastError(), "The download file could not be opened.")));
    }
    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file.h, position, nullptr, FILE_BEGIN) || !SetEndOfFile(file.h)) {
        return std::unexpected(fail(Error::fromWin32(ErrorCategory::Filesystem, "download", GetLastError(), "The download file could not be positioned.")));
    }

    std::vector<char> buffer(kChunk);
    std::uint64_t done = offset;
    SpeedMeter meter;
    auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(1);
    auto report = [&](bool force) {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - lastReport < std::chrono::milliseconds(120)) {
            return;
        }
        lastReport = now;
        if (progress) {
            DownloadProgress p;
            p.done = done;
            p.total = total;
            p.bytesPerSecond = meter.bytesPerSecond();
            p.etaSeconds = (p.bytesPerSecond > 0 && total > done) ? static_cast<double>(total - done) / p.bytesPerSecond : 0.0;
            progress(p);
        }
    };
    report(true);

    while (true) {
        if (token.stop_requested()) {
            return std::unexpected(fail(Error::cancelled("download")));
        }
        DWORD read = 0;
        if (!WinHttpReadData(req.get(), buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            if (token.stop_requested()) {
                return std::unexpected(fail(Error::cancelled("download")));
            }
            return std::unexpected(fail(networkError(GetLastError(), "The connection was interrupted.")));
        }
        if (read == 0) {
            break;
        }
        DWORD written = 0;
        if (!WriteFile(file.h, buffer.data(), read, &written, nullptr) || written != read) {
            return std::unexpected(fail(Error::fromWin32(ErrorCategory::Filesystem, "download", GetLastError(), "The download could not be written to disk.")));
        }
        done += read;
        meter.add(read);
        if (total != 0 && done > total) {
            return std::unexpected(fail(Error::make(ErrorCategory::Download, "download", "The server sent more data than expected.", url)));
        }
        report(false);
    }
    FlushFileBuffers(file.h);
    report(true);

    if (total != 0 && done != total) {
        Error e = Error::make(ErrorCategory::Download, "download", "The connection closed before the download was complete.",
                              std::format("{} of {} bytes", done, total), true);
        return std::unexpected(fail(e));
    }
    return {};
}

}

Result<void> downloadFile(const DownloadRequest& request, std::stop_token token, const DownloadProgressSink& progress) {
    if (request.urls.empty()) {
        return std::unexpected(Error::make(ErrorCategory::Download, "download", "No download address is known for this version."));
    }
    std::error_code ec;
    std::filesystem::create_directories(request.partialFile.parent_path(), ec);

    Error last;
    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        if (token.stop_requested()) {
            return std::unexpected(Error::cancelled("download"));
        }
        const std::string& url = request.urls[static_cast<size_t>(attempt) % request.urls.size()];
        log::info("download attempt {} from {}", attempt + 1, url);
        auto result = runAttempt(request, url, token, progress);
        if (result) {
            return {};
        }
        last = result.error().error;
        if (last.isCancelled()) {
            return std::unexpected(last);
        }
        log::warn("download attempt {} failed: {}", attempt + 1, last.summary());
        if (!result.error().retry) {
            return std::unexpected(last);
        }
        const auto delay = std::chrono::milliseconds(500 * (attempt + 1));
        const auto until = std::chrono::steady_clock::now() + delay;
        while (std::chrono::steady_clock::now() < until) {
            if (token.stop_requested()) {
                return std::unexpected(Error::cancelled("download"));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    return std::unexpected(last);
}

}

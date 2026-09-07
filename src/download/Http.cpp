#include "download/Http.h"

#include "core/Text.h"

#include <windows.h>
#include <winhttp.h>

#include <format>
#include <vector>

namespace citron::http {

namespace {

struct Handle {
    HINTERNET h = nullptr;
    ~Handle() {
        if (h != nullptr) {
            WinHttpCloseHandle(h);
        }
    }
};

Error httpError(const char* operation, DWORD code, std::string message) {
    Error e = Error::fromWin32(ErrorCategory::Network, operation, code, std::move(message));
    e.retryable = true;
    return e;
}

}

const wchar_t* userAgent() {
    return L"CitronLauncher/0.1";
}

std::optional<Url> parseUrl(std::string_view url) {
    const std::wstring wide = text::toWide(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide.c_str(), static_cast<DWORD>(wide.size()), 0, &parts)) {
        return std::nullopt;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) {
        return std::nullopt;
    }
    Url out;
    out.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    out.path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.lpszExtraInfo != nullptr && parts.dwExtraInfoLength > 0) {
        out.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }
    if (out.path.empty()) {
        out.path = L"/";
    }
    out.port = parts.nPort;
    out.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    return out;
}

Result<std::string> get(std::string_view url, std::uint64_t maxBytes, std::stop_token token) {
    const auto parsed = parseUrl(url);
    if (!parsed) {
        return std::unexpected(Error::make(ErrorCategory::Network, "http get", "The address is not valid.", std::string(url)));
    }
    Handle session;
    session.h = WinHttpOpen(userAgent(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (session.h == nullptr) {
        return std::unexpected(httpError("http get", GetLastError(), "The network session could not be opened."));
    }
    WinHttpSetTimeouts(session.h, 10000, 15000, 30000, 30000);
    Handle connection;
    connection.h = WinHttpConnect(session.h, parsed->host.c_str(), parsed->port, 0);
    if (connection.h == nullptr) {
        return std::unexpected(httpError("http get", GetLastError(), "The server could not be reached."));
    }
    Handle request;
    request.h = WinHttpOpenRequest(connection.h, L"GET", parsed->path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   parsed->secure ? WINHTTP_FLAG_SECURE : 0);
    if (request.h == nullptr) {
        return std::unexpected(httpError("http get", GetLastError(), "The request could not be created."));
    }
    const wchar_t* headers = L"Accept: application/json, */*\r\nCache-Control: no-cache\r\n";
    if (!WinHttpSendRequest(request.h, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request.h, nullptr)) {
        return std::unexpected(httpError("http get", GetLastError(), "The server did not respond."));
    }
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status != 200) {
        Error e = Error::make(ErrorCategory::Network, "http get", std::format("The server answered with status {}.", status), std::string(url));
        e.httpStatus = status;
        e.retryable = status >= 500;
        return std::unexpected(e);
    }
    std::string body;
    std::vector<char> buffer(64 * 1024);
    while (true) {
        if (token.stop_requested()) {
            return std::unexpected(Error::cancelled("http get"));
        }
        DWORD read = 0;
        if (!WinHttpReadData(request.h, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            return std::unexpected(httpError("http get", GetLastError(), "The response could not be read."));
        }
        if (read == 0) {
            break;
        }
        body.append(buffer.data(), read);
        if (body.size() > maxBytes) {
            return std::unexpected(Error::make(ErrorCategory::Network, "http get", "The response is larger than expected.", std::string(url)));
        }
    }
    return body;
}

}

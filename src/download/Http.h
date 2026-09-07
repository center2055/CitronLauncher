#pragma once

#include "core/Error.h"

#include <cstdint>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>

namespace citron::http {

struct Url {
    std::wstring host;
    std::wstring path;
    unsigned short port = 80;
    bool secure = false;
};

std::optional<Url> parseUrl(std::string_view url);
Result<std::string> get(std::string_view url, std::uint64_t maxBytes, std::stop_token token);
const wchar_t* userAgent();

}

#pragma once

#include "core/Error.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>

namespace citron::platform {

using HashProgress = std::function<void(std::uint64_t done, std::uint64_t total)>;

Result<std::string> md5OfFile(const std::filesystem::path& file, std::stop_token token, const HashProgress& progress = {});

}

#pragma once

#include "core/Error.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace citron {

struct DownloadRequest {
    std::vector<std::string> urls;
    std::filesystem::path partialFile;
    std::uint64_t expectedSize = 0;
    std::string md5;
};

struct DownloadProgress {
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    double bytesPerSecond = 0.0;
    double etaSeconds = 0.0;
};

using DownloadProgressSink = std::function<void(const DownloadProgress&)>;

Result<void> downloadFile(const DownloadRequest& request, std::stop_token token, const DownloadProgressSink& progress);

}

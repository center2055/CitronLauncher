#pragma once

#include "core/Error.h"
#include "core/Json.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace citron {

struct DownloadMeta {
    std::string url;
    std::string etag;
    std::string lastModified;
    std::uint64_t totalSize = 0;
    std::string md5;
    std::int64_t started = 0;

    json::Value toJson() const;
    static std::optional<DownloadMeta> fromJson(const json::Value& value);
};

std::filesystem::path metaPathFor(const std::filesystem::path& partialFile);
std::optional<DownloadMeta> readDownloadMeta(const std::filesystem::path& partialFile);
Result<void> writeDownloadMeta(const std::filesystem::path& partialFile, const DownloadMeta& meta);
void removeDownloadMeta(const std::filesystem::path& partialFile);

}

#pragma once

#include "core/Error.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace citron::platform {

std::optional<std::uint64_t> fileSize(const std::filesystem::path& file);
bool fileExists(const std::filesystem::path& file);
Result<void> moveReplace(const std::filesystem::path& from, const std::filesystem::path& to);
Result<void> moveDirectory(const std::filesystem::path& from, const std::filesystem::path& to);
Result<void> removeFile(const std::filesystem::path& file);
Result<void> removeDirectoryTree(const std::filesystem::path& directory);
void discardFile(const std::filesystem::path& file);
std::optional<std::uint64_t> freeSpace(const std::filesystem::path& directory);
std::filesystem::path finalPath(const std::filesystem::path& path);
std::int64_t unixNow();
bool isWritableDirectory(const std::filesystem::path& directory);

}

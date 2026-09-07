#pragma once

#include <filesystem>
#include <optional>
#include <string_view>

namespace citron::pathsafety {

bool isSafeFileName(std::string_view name);
bool isInside(const std::filesystem::path& root, const std::filesystem::path& target);
std::optional<std::filesystem::path> childOf(const std::filesystem::path& root, std::string_view name);
std::filesystem::path normalize(const std::filesystem::path& path);

}

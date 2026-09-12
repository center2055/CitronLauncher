#pragma once

#include "core/Error.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>

namespace citron {

using ExtractionProgressSink = std::function<void(std::uint64_t current, std::uint64_t total)>;

// Keeps the MSIXVC native ABI at the boundary of the Minecraft layer. Nothing
// above this interface knows about DLL exports, DLL search paths, or pipes.
class IGdkPackageExtractor {
public:
    virtual ~IGdkPackageExtractor() = default;

    virtual Result<void> extract(const std::filesystem::path& package,
                                 const std::filesystem::path& outputDirectory,
                                 const std::filesystem::path& nativeDirectory,
                                 ExtractionProgressSink progress,
                                 std::stop_token cancellation) = 0;
};

class LauncherCoreGdkExtractor final : public IGdkPackageExtractor {
public:
    Result<void> extract(const std::filesystem::path& package,
                         const std::filesystem::path& outputDirectory,
                         const std::filesystem::path& nativeDirectory,
                         ExtractionProgressSink progress,
                         std::stop_token cancellation) override;
};

}

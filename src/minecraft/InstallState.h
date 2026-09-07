#pragma once

#include "core/Error.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace citron {

enum class InstallStage {
    Idle,
    Resolving,
    Downloading,
    Verifying,
    Finalizing,
    Deploying,
    Completed,
    Failed,
    Cancelled,
};

std::string_view stageName(InstallStage stage);
bool isTerminal(InstallStage stage);
bool isBusy(InstallStage stage);
bool canTransition(InstallStage from, InstallStage to);

struct InstallProgress {
    InstallStage stage = InstallStage::Idle;
    std::uint64_t done = 0;
    std::uint64_t total = 0;
    double bytesPerSecond = 0.0;
    double etaSeconds = 0.0;
    std::optional<Error> error;

    double fraction() const;
};

enum class ResumeDecision {
    Resume,
    Restart,
};

struct PartialFile {
    std::uint64_t size = 0;
    std::string url;
    std::string etag;
    std::string lastModified;
    std::uint64_t totalSize = 0;
    std::string md5;
};

ResumeDecision decideResume(const PartialFile& partial, std::string_view url, std::uint64_t expectedTotal, std::string_view md5);

}

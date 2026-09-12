#include "minecraft/InstallState.h"

#include "core/Text.h"

namespace citron {

std::string_view stageName(InstallStage stage) {
    switch (stage) {
    case InstallStage::Idle: return "idle";
    case InstallStage::Resolving: return "resolving";
    case InstallStage::Downloading: return "downloading";
    case InstallStage::Verifying: return "verifying";
    case InstallStage::Finalizing: return "finalizing";
    case InstallStage::Downloaded: return "downloaded";
    case InstallStage::Extracting: return "extracting";
    case InstallStage::Removing: return "removing";
    case InstallStage::Completed: return "completed";
    case InstallStage::Failed: return "failed";
    case InstallStage::Cancelled: return "cancelled";
    }
    return "unknown";
}

bool isTerminal(InstallStage stage) {
    return stage == InstallStage::Downloaded || stage == InstallStage::Completed || stage == InstallStage::Failed || stage == InstallStage::Cancelled;
}

bool isBusy(InstallStage stage) {
    return stage != InstallStage::Idle && !isTerminal(stage);
}

bool canTransition(InstallStage from, InstallStage to) {
    if (from == to) {
        return false;
    }
    if (isTerminal(from)) {
        return to == InstallStage::Idle;
    }
    if (to == InstallStage::Failed || to == InstallStage::Cancelled) {
        return from != InstallStage::Idle;
    }
    switch (from) {
    case InstallStage::Idle: return to == InstallStage::Resolving || to == InstallStage::Removing;
    case InstallStage::Resolving: return to == InstallStage::Downloading || to == InstallStage::Verifying;
    case InstallStage::Downloading: return to == InstallStage::Verifying;
    case InstallStage::Verifying: return to == InstallStage::Finalizing || to == InstallStage::Extracting;
    case InstallStage::Finalizing: return to == InstallStage::Downloaded || to == InstallStage::Extracting || to == InstallStage::Completed;
    case InstallStage::Extracting: return to == InstallStage::Completed;
    case InstallStage::Removing: return to == InstallStage::Idle;
    default: return false;
    }
}

double InstallProgress::fraction() const {
    if (total == 0) {
        return 0.0;
    }
    const double f = static_cast<double>(done) / static_cast<double>(total);
    return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
}

ResumeDecision decideResume(const PartialFile& partial, std::string_view url, std::uint64_t expectedTotal, std::string_view md5) {
    if (partial.size == 0) {
        return ResumeDecision::Restart;
    }
    if (partial.url != url) {
        return ResumeDecision::Restart;
    }
    if (!text::equalsIgnoreCase(partial.md5, md5)) {
        return ResumeDecision::Restart;
    }
    if (expectedTotal != 0 && partial.totalSize != 0 && partial.totalSize != expectedTotal) {
        return ResumeDecision::Restart;
    }
    if (expectedTotal != 0 && partial.size >= expectedTotal) {
        return ResumeDecision::Restart;
    }
    return ResumeDecision::Resume;
}

}

#include "minecraft/InstallationManager.h"

#include "core/Logger.h"
#include "core/PathSafety.h"
#include "core/Text.h"
#include "download/DownloadMeta.h"
#include "platform/windows/FileOps.h"
#include "platform/windows/Hash.h"

#include <chrono>
#include <thread>

namespace citron {

namespace {

constexpr std::uint64_t kSpareBytes = 512ull * 1024 * 1024;

std::uint64_t saturatedAdd(std::uint64_t a, std::uint64_t b) {
    return a > UINT64_MAX - b ? UINT64_MAX : a + b;
}

}

InstallationManager::InstallationManager(VersionManager& versions, TaskScheduler& scheduler, DownloadManager& downloads)
    : versions_(versions), scheduler_(scheduler), downloads_(downloads), extractor_(std::make_unique<LauncherCoreGdkExtractor>()) {}

void InstallationManager::setProgressSink(InstallProgressSink sink) {
    std::lock_guard lock(mutex_);
    sink_ = std::move(sink);
}

void InstallationManager::report(const VersionId& id, const InstallProgress& progress) {
    InstallProgressSink sink;
    {
        std::lock_guard lock(mutex_);
        sink = sink_;
    }
    if (sink) {
        sink(id, progress);
    }
}

void InstallationManager::finish(const VersionId& id) {
    std::lock_guard lock(mutex_);
    operations_.erase(id);
}

void InstallationManager::extractVerifiedPackage(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& package, std::stop_token token) {
    const auto destination = versions_.managedPath(id);
    const auto staging = versions_.stagingPath(id);
    InstallProgress p;
    p.stage = InstallStage::Extracting;
    p.total = 1;
    report(id, p);

    if (!pathsafety::isInside(versions_.layout().versions, destination) || !pathsafety::isInside(versions_.layout().versions, staging)) {
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::Filesystem, "extract", "The version extraction location is outside the launcher folder.");
        report(id, p);
        finish(id);
        return;
    }
    std::error_code ec;
    if (std::filesystem::exists(destination, ec)) {
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::Filesystem, "extract", "This version already has an incomplete installation directory.",
                              destination.string(), true);
        report(id, p);
        finish(id);
        return;
    }
    if (auto removed = platform::removeDirectoryTree(staging); !removed) {
        p.stage = InstallStage::Failed;
        p.error = removed.error();
        report(id, p);
        finish(id);
        return;
    }
    std::filesystem::create_directories(staging, ec);
    if (ec) {
        p.stage = InstallStage::Failed;
        p.error = Error::fromWin32(ErrorCategory::Filesystem, "create extraction directory", static_cast<unsigned long>(ec.value()),
                                   "Citron could not create the version staging directory.");
        report(id, p);
        finish(id);
        return;
    }

    log::info("extracting {} into {}", id.key(), staging.string());
    const auto nativeDirectory = versions_.layout().root / L"native" / L"launchercore";
    auto extracted = extractor_->extract(package, staging, nativeDirectory, [this, id](std::uint64_t current, std::uint64_t total) {
        InstallProgress progress;
        progress.stage = InstallStage::Extracting;
        progress.done = current;
        progress.total = total;
        report(id, progress);
    }, token);
    if (!extracted) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = extracted.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
        p.error = extracted.error();
        report(id, p);
        finish(id);
        return;
    }
    if (token.stop_requested()) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = InstallStage::Cancelled;
        p.error = Error::cancelled("extract");
        report(id, p);
        finish(id);
        return;
    }
    platform::initializeApartment();
    if (auto prepared = versions_.prepareManagedInstallation(id, staging, token); !prepared) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = prepared.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
        p.error = prepared.error();
        report(id, p);
        finish(id);
        return;
    }
    if (!versions_.isCompleteManagedInstallation(staging)) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::Package, "validate extraction", "The extracted version is incomplete.",
                              "Citron could not finish provisioning the required GDK runtime files or launcher manifest.", true);
        report(id, p);
        finish(id);
        return;
    }
    if (auto metadata = versions_.writeManagedMetadata(id, staging, entry.md5); !metadata) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = InstallStage::Failed;
        p.error = metadata.error();
        report(id, p);
        finish(id);
        return;
    }
    if (auto promoted = platform::moveDirectory(staging, destination); !promoted) {
        static_cast<void>(platform::removeDirectoryTree(staging));
        p.stage = InstallStage::Failed;
        p.error = promoted.error();
        report(id, p);
        finish(id);
        return;
    }
    if (auto removed = platform::removeFile(package); !removed) {
        log::warn("verified package cache could not be removed after extracting {}: {}", id.key(), removed.error().summary());
    }
    p.stage = InstallStage::Completed;
    p.done = 1;
    p.total = 1;
    report(id, p);
    finish(id);
}

bool InstallationManager::busy(const VersionId& id) const {
    std::lock_guard lock(mutex_);
    auto it = operations_.find(id);
    return (it != operations_.end() && !it->second->done()) || downloads_.active(id);
}

bool InstallationManager::anyBusy() const {
    std::lock_guard lock(mutex_);
    for (const auto& [id, op] : operations_) {
        if (!op->done()) {
            return true;
        }
    }
    return downloads_.activeCount() > 0;
}

void InstallationManager::cancel(const VersionId& id) {
    downloads_.cancel(id);
    std::shared_ptr<Operation> op;
    {
        std::lock_guard lock(mutex_);
        if (auto it = operations_.find(id); it != operations_.end()) {
            op = it->second;
        }
    }
    if (op) {
        op->cancel();
    }
}

bool InstallationManager::install(const VersionId& id, const CatalogEntry& entry, std::vector<std::string> urls) {
    if (busy(id)) {
        return false;
    }
    const auto partial = versions_.partialPath(id);
    const auto target = versions_.packagePath(id);
    if (!pathsafety::isInside(versions_.layout().root, partial) || !pathsafety::isInside(versions_.layout().root, target) ||
        !pathsafety::isInside(versions_.layout().versions, versions_.managedPath(id))) {
        InstallProgress p;
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::Filesystem, "install", "The download location is outside the launcher folder.");
        report(id, p);
        return false;
    }
    const bool cachedPackage = platform::fileExists(target);
    if (const auto free = platform::freeSpace(versions_.layout().root)) {
        const std::uint64_t have = platform::fileSize(partial).value_or(0);
        const std::uint64_t required = saturatedAdd(entry.size, kSpareBytes);
        if (saturatedAdd(*free, have) < required) {
            InstallProgress p;
            p.stage = InstallStage::Failed;
            p.error = Error::make(ErrorCategory::Filesystem, "install", "There is not enough free disk space for this version.",
                                  "free " + std::to_string(*free / (1024 * 1024)) + " MB, needed " + std::to_string(required / (1024 * 1024)) + " MB");
            report(id, p);
            return false;
        }
    }

    InstallProgress starting;
    starting.stage = InstallStage::Resolving;
    starting.total = entry.size;
    report(id, starting);

    // second press: the verified package is already in the installer cache, so extract it into an isolated version.
    if (cachedPackage) {
        std::lock_guard lock(mutex_);
        operations_[id] = scheduler_.start("install " + id.key(), [this, id, entry, target](std::stop_token token) {
            verifyAndExtract(id, entry, target, token);
        });
        return true;
    }

    DownloadRequest request;
    request.urls = std::move(urls);
    request.partialFile = partial;
    request.expectedSize = entry.size;
    request.md5 = entry.md5;

    auto update = [this](const VersionId& vid, const DownloadProgress& dp) {
        InstallProgress p;
        p.stage = InstallStage::Downloading;
        p.done = dp.done;
        p.total = dp.total;
        p.bytesPerSecond = dp.bytesPerSecond;
        p.etaSeconds = dp.etaSeconds;
        report(vid, p);
    };

    auto done = [this, entry, partial, target](const VersionId& vid, Result<void> result) {
        if (!result) {
            InstallProgress p;
            p.stage = result.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
            p.error = result.error();
            report(vid, p);
            return;
        }
        std::lock_guard lock(mutex_);
        operations_[vid] = scheduler_.start("verify " + vid.key(), [this, vid, entry, partial, target](std::stop_token token) {
            verifyDownload(vid, entry, partial, target, token);
        });
    };

    return downloads_.start(id, std::move(request), std::move(update), std::move(done));
}

Result<void> InstallationManager::verifyChecksum(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& source, std::stop_token token) {
    InstallProgress p;
    p.stage = InstallStage::Verifying;
    p.total = entry.size;
    report(id, p);
    log::info("verifying {}", id.key());
    auto hash = platform::md5OfFile(source, token, [this, &id](std::uint64_t done, std::uint64_t total) {
        InstallProgress hp;
        hp.stage = InstallStage::Verifying;
        hp.done = done;
        hp.total = total;
        report(id, hp);
    });
    if (!hash) {
        return std::unexpected(hash.error());
    }
    if (!text::equalsIgnoreCase(*hash, entry.md5)) {
        log::error("checksum mismatch for {}: expected {} got {}", id.key(), entry.md5, *hash);
        return std::unexpected(Error::make(ErrorCategory::Download, "verify", "The downloaded package is damaged (checksum mismatch).", *hash, true));
    }
    return {};
}

// download step: verify the freshly downloaded package, move it into the installer cache, and stop.
void InstallationManager::verifyDownload(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& partial, const std::filesystem::path& target, std::stop_token token) {
    if (auto verified = verifyChecksum(id, entry, partial, token); !verified) {
        if (!verified.error().isCancelled()) {
            platform::discardFile(partial);
            removeDownloadMeta(partial);
        }
        InstallProgress p;
        p.stage = verified.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
        p.error = verified.error();
        report(id, p);
        finish(id);
        return;
    }
    InstallProgress p;
    p.stage = InstallStage::Finalizing;
    p.done = entry.size;
    p.total = entry.size;
    report(id, p);
    if (auto moved = platform::moveReplace(partial, target); !moved) {
        p.stage = InstallStage::Failed;
        p.error = moved.error();
        report(id, p);
        finish(id);
        return;
    }
    removeDownloadMeta(partial);
    log::info("downloaded {} to {}", id.key(), target.string());
    p.stage = InstallStage::Downloaded;
    p.done = entry.size;
    p.total = entry.size;
    report(id, p);
    finish(id);
}

// install step: verify the cached package, then extract it into an isolated version.
void InstallationManager::verifyAndExtract(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& target, std::stop_token token) {
    if (auto verified = verifyChecksum(id, entry, target, token); !verified) {
        if (!verified.error().isCancelled()) {
            platform::discardFile(target);
        }
        InstallProgress p;
        p.stage = verified.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
        p.error = verified.error();
        report(id, p);
        finish(id);
        return;
    }
    extractVerifiedPackage(id, entry, target, token);
}

bool InstallationManager::removeDeployment(const VersionId& id, std::wstring fullName, RemoveDone done) {
    if (busy(id)) {
        return false;
    }
    auto job = [this, id, fullName = std::move(fullName), done = std::move(done)](std::stop_token token) {
        platform::initializeApartment();
        log::info("removing deployment {}", text::toUtf8(fullName));
        auto result = platform::removePackage(fullName, token);
        finish(id);
        if (done) {
            done(id, std::move(result));
        }
    };
    std::lock_guard lock(mutex_);
    operations_[id] = scheduler_.start("remove " + id.key(), std::move(job));
    return true;
}

}

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

constexpr int kDeployAttempts = 3;
constexpr std::uint64_t kSpareBytes = 512ull * 1024 * 1024;

}

InstallationManager::InstallationManager(VersionManager& versions, TaskScheduler& scheduler, DownloadManager& downloads)
    : versions_(versions), scheduler_(scheduler), downloads_(downloads) {}

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
    if (!pathsafety::isInside(versions_.layout().root, partial) || !pathsafety::isInside(versions_.layout().root, target)) {
        InstallProgress p;
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::Filesystem, "install", "The download location is outside the launcher folder.");
        report(id, p);
        return false;
    }
    if (const auto free = platform::freeSpace(versions_.layout().root)) {
        const std::uint64_t have = platform::fileSize(partial).value_or(0);
        if (*free + have < entry.size + kSpareBytes) {
            InstallProgress p;
            p.stage = InstallStage::Failed;
            p.error = Error::make(ErrorCategory::Filesystem, "install", "There is not enough free disk space for this version.",
                                  "free " + std::to_string(*free / (1024 * 1024)) + " MB, needed " + std::to_string((entry.size + kSpareBytes) / (1024 * 1024)) + " MB");
            report(id, p);
            return false;
        }
    }

    InstallProgress starting;
    starting.stage = InstallStage::Resolving;
    starting.total = entry.size;
    report(id, starting);

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
        auto job = [this, vid, entry, partial, target](std::stop_token token) {
            InstallProgress p;
            p.stage = InstallStage::Verifying;
            p.total = entry.size;
            report(vid, p);
            log::info("verifying {}", vid.key());
            auto hash = platform::md5OfFile(partial, token, [&](std::uint64_t done, std::uint64_t total) {
                InstallProgress hp;
                hp.stage = InstallStage::Verifying;
                hp.done = done;
                hp.total = total;
                report(vid, hp);
            });
            if (!hash) {
                p.stage = hash.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
                p.error = hash.error();
                report(vid, p);
                finish(vid);
                return;
            }
            if (!text::equalsIgnoreCase(*hash, entry.md5)) {
                log::error("checksum mismatch for {}: expected {} got {}", vid.key(), entry.md5, *hash);
                platform::discardFile(partial);
                removeDownloadMeta(partial);
                p.stage = InstallStage::Failed;
                p.error = Error::make(ErrorCategory::Download, "verify", "The downloaded package is damaged (checksum mismatch).", *hash, true);
                report(vid, p);
                finish(vid);
                return;
            }
            p.stage = InstallStage::Finalizing;
            p.done = entry.size;
            report(vid, p);
            if (auto moved = platform::moveReplace(partial, target); !moved) {
                p.stage = InstallStage::Failed;
                p.error = moved.error();
                report(vid, p);
                finish(vid);
                return;
            }
            removeDownloadMeta(partial);
            log::info("installed package {} at {}", vid.key(), target.string());
            p.stage = InstallStage::Completed;
            report(vid, p);
            finish(vid);
        };
        std::lock_guard lock(mutex_);
        operations_[vid] = scheduler_.start("verify " + vid.key(), std::move(job));
    };

    return downloads_.start(id, std::move(request), std::move(update), std::move(done));
}

bool InstallationManager::activate(const VersionId& id, std::filesystem::path package, std::optional<std::wstring> fallbackReplace, ActivateDone done) {
    if (busy(id)) {
        return false;
    }
    auto job = [this, id, package = std::move(package), fallbackReplace = std::move(fallbackReplace), done = std::move(done)](std::stop_token token) {
        InstallProgress p;
        p.stage = InstallStage::Deploying;
        p.total = 100;
        report(id, p);
        platform::initializeApartment();

        auto deploy = [&] {
            Result<platform::InstalledPackage> outcome = std::unexpected(Error::make(ErrorCategory::Package, "deploy", "Minecraft could not be installed."));
            for (int attempt = 1; attempt <= kDeployAttempts; ++attempt) {
                if (token.stop_requested()) {
                    return Result<platform::InstalledPackage>(std::unexpected(Error::cancelled("deploy")));
                }
                log::info("deploying {} (attempt {})", id.key(), attempt);
                outcome = platform::deployPackage(package, token, [&](int percent) {
                    InstallProgress dp;
                    dp.stage = InstallStage::Deploying;
                    dp.done = static_cast<std::uint64_t>(percent);
                    dp.total = 100;
                    report(id, dp);
                });
                if (outcome || outcome.error().isCancelled() || !outcome.error().retryable) {
                    break;
                }
                log::warn("deploy attempt {} failed: {}", attempt, outcome.error().summary());
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
            return outcome;
        };

        // windows replaces the installed version of a package family in place, so the
        // previous one is only removed if that fails
        Result<platform::InstalledPackage> result = deploy();
        if (!result && !result.error().isCancelled() && fallbackReplace) {
            log::warn("in place replacement failed, removing {} first: {}", text::toUtf8(*fallbackReplace), result.error().summary());
            if (auto removed = platform::removePackage(*fallbackReplace, token); removed) {
                versions_.clearDeployRecord(id.channel);
                result = deploy();
            } else if (!removed.error().isCancelled()) {
                log::warn("previous version could not be removed: {}", removed.error().summary());
            }
        }

        if (!result) {
            p.stage = result.error().isCancelled() ? InstallStage::Cancelled : InstallStage::Failed;
            p.error = result.error();
            report(id, p);
            finish(id);
            if (done) {
                done(id, std::unexpected(result.error()));
            }
            return;
        }
        DeployRecord record;
        record.fullName = text::toUtf8(result->fullName);
        record.version = id.number.toString();
        versions_.saveDeployRecord(id.channel, record);
        log::info("deployed {} at {}", id.key(), result->installLocation.string());
        p.stage = InstallStage::Completed;
        p.done = 100;
        report(id, p);
        finish(id);
        if (done) {
            done(id, std::move(result));
        }
    };
    std::lock_guard lock(mutex_);
    operations_[id] = scheduler_.start("activate " + id.key(), std::move(job));
    return true;
}

bool InstallationManager::removeDeployment(const VersionId& id, std::wstring fullName, RemoveDone done) {
    if (busy(id)) {
        return false;
    }
    auto job = [this, id, fullName = std::move(fullName), done = std::move(done)](std::stop_token token) {
        platform::initializeApartment();
        log::info("removing deployment {}", text::toUtf8(fullName));
        auto result = platform::removePackage(fullName, token);
        if (result) {
            versions_.clearDeployRecord(id.channel);
        }
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

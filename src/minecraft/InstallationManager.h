#pragma once

#include "core/TaskScheduler.h"
#include "download/DownloadManager.h"
#include "minecraft/GdkPackageExtractor.h"
#include "minecraft/InstallState.h"
#include "minecraft/Version.h"
#include "minecraft/VersionCatalog.h"
#include "minecraft/VersionManager.h"
#include "platform/windows/PackageManager.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace citron {

using InstallProgressSink = std::function<void(const VersionId& id, const InstallProgress& progress)>;
using RemoveDone = std::function<void(const VersionId& id, Result<void> result)>;

class InstallationManager {
public:
    InstallationManager(VersionManager& versions, TaskScheduler& scheduler, DownloadManager& downloads);

    void setProgressSink(InstallProgressSink sink);

    bool install(const VersionId& id, const CatalogEntry& entry, std::vector<std::string> urls);
    bool removeDeployment(const VersionId& id, std::wstring fullName, RemoveDone done);
    void cancel(const VersionId& id);
    bool busy(const VersionId& id) const;
    bool anyBusy() const;

private:
    void report(const VersionId& id, const InstallProgress& progress);
    void finish(const VersionId& id);
    Result<void> verifyChecksum(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& source, std::stop_token token);
    void verifyDownload(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& partial, const std::filesystem::path& target, std::stop_token token);
    void verifyAndExtract(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& target, std::stop_token token);
    void extractVerifiedPackage(const VersionId& id, const CatalogEntry& entry, const std::filesystem::path& package, std::stop_token token);

    VersionManager& versions_;
    TaskScheduler& scheduler_;
    DownloadManager& downloads_;
    std::unique_ptr<IGdkPackageExtractor> extractor_;
    InstallProgressSink sink_;
    mutable std::mutex mutex_;
    std::map<VersionId, std::shared_ptr<Operation>> operations_;
};

}

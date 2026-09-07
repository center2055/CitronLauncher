#pragma once

#include "core/TaskScheduler.h"
#include "download/DownloadManager.h"
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
using ActivateDone = std::function<void(const VersionId& id, Result<platform::InstalledPackage> result)>;
using RemoveDone = std::function<void(const VersionId& id, Result<void> result)>;

class InstallationManager {
public:
    InstallationManager(VersionManager& versions, TaskScheduler& scheduler, DownloadManager& downloads);

    void setProgressSink(InstallProgressSink sink);

    bool install(const VersionId& id, const CatalogEntry& entry, std::vector<std::string> urls);
    bool activate(const VersionId& id, std::filesystem::path package, std::optional<std::wstring> replaceFullName, ActivateDone done);
    bool removeDeployment(const VersionId& id, std::wstring fullName, RemoveDone done);
    void cancel(const VersionId& id);
    bool busy(const VersionId& id) const;
    bool anyBusy() const;

private:
    void report(const VersionId& id, const InstallProgress& progress);
    void finish(const VersionId& id);

    VersionManager& versions_;
    TaskScheduler& scheduler_;
    DownloadManager& downloads_;
    InstallProgressSink sink_;
    mutable std::mutex mutex_;
    std::map<VersionId, std::shared_ptr<Operation>> operations_;
};

}

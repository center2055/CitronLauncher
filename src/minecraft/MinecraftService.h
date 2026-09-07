#pragma once

#include "core/Error.h"
#include "core/Paths.h"
#include "core/TaskScheduler.h"
#include "download/DownloadManager.h"
#include "minecraft/InstallState.h"
#include "minecraft/InstallationManager.h"
#include "minecraft/LaunchManager.h"
#include "minecraft/PrerequisiteManager.h"
#include "minecraft/Version.h"
#include "minecraft/VersionCatalog.h"
#include "minecraft/VersionManager.h"
#include "platform/windows/Gdk.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace citron {

struct VersionInfo {
    VersionId id;
    std::optional<CatalogEntry> catalog;
    std::optional<std::filesystem::path> packageFile;
    std::uint64_t packageSize = 0;
    bool deployed = false;
    bool deployedByCitron = false;
    std::wstring deployedFullName;
    std::filesystem::path deployedLocation;
    std::uint64_t partialSize = 0;

    bool installed() const { return packageFile.has_value() || deployed; }
    std::uint64_t size() const;
};

struct ServiceSnapshot {
    std::vector<VersionInfo> versions;
    std::map<VersionId, InstallProgress> operations;
    bool catalogLoading = false;
    std::optional<Error> catalogError;
    std::int64_t catalogUpdated = 0;
    std::optional<platform::GdkEnvironment> environment;
    bool launching = false;
};

class MinecraftService {
public:
    MinecraftService(paths::Layout layout, TaskScheduler& scheduler, Dispatcher& dispatcher, std::string embeddedCatalog);
    ~MinecraftService();

    void setChangeHandler(std::function<void()> handler);
    ServiceSnapshot snapshot() const;
    const paths::Layout& layout() const { return versions_.layout(); }

    void start(std::string_view catalogUrl);
    void refreshInstalled();
    void refreshCatalog(std::string_view catalogUrl);
    void checkEnvironment();

    void install(const VersionId& id);
    void cancel(const VersionId& id);
    void dismiss(const VersionId& id);
    void remove(const VersionId& id, std::function<void(Result<void>)> done);
    void activate(const VersionId& id, bool keepPackage, std::function<void(Result<std::filesystem::path>)> done);
    void launch(const VersionId& id, bool keepPackage, std::function<void(Result<void>)> done);
    bool busy(const VersionId& id) const;
    bool anyBusy() const;

    std::optional<VersionInfo> find(const VersionId& id) const;
    std::optional<VersionInfo> deployedFor(VersionChannel channel) const;

    Result<void> setRoot(const paths::Layout& layout);

private:
    void rebuild();
    void notify();
    void updateOperation(const VersionId& id, const InstallProgress& progress);
    void reportLaunch(std::function<void(Result<void>)> done, Result<void> result);

    TaskScheduler& scheduler_;
    Dispatcher& dispatcher_;
    VersionManager versions_;
    DownloadManager downloads_;
    InstallationManager installs_;
    LaunchManager launcher_;
    PrerequisiteManager prerequisites_;
    std::string embeddedCatalog_;

    mutable std::mutex mutex_;
    Catalog catalog_;
    std::vector<PackageFile> packages_;
    std::vector<PartialDownload> partials_;
    std::vector<DeployedPackage> deployed_;
    ServiceSnapshot snapshot_;
    std::function<void()> changeHandler_;
    bool notifyPending_ = false;
};

}

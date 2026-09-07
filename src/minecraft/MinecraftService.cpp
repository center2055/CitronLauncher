#include "minecraft/MinecraftService.h"

#include "core/Logger.h"
#include "core/PathSafety.h"
#include "core/Text.h"
#include "download/DownloadMeta.h"
#include "platform/windows/FileOps.h"
#include "platform/windows/PackageManager.h"

#include <algorithm>

namespace citron {

std::uint64_t VersionInfo::size() const {
    if (packageSize != 0) {
        return packageSize;
    }
    if (catalog) {
        return catalog->size;
    }
    return 0;
}

MinecraftService::MinecraftService(paths::Layout layout, TaskScheduler& scheduler, Dispatcher& dispatcher, std::string embeddedCatalog)
    : scheduler_(scheduler), dispatcher_(dispatcher), versions_(std::move(layout)), downloads_(scheduler), installs_(versions_, scheduler, downloads_),
      launcher_(scheduler), prerequisites_(scheduler), embeddedCatalog_(std::move(embeddedCatalog)) {
    installs_.setProgressSink([this](const VersionId& id, const InstallProgress& progress) { updateOperation(id, progress); });
}

MinecraftService::~MinecraftService() = default;

void MinecraftService::setChangeHandler(std::function<void()> handler) {
    std::lock_guard lock(mutex_);
    changeHandler_ = std::move(handler);
}

ServiceSnapshot MinecraftService::snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

std::optional<VersionInfo> MinecraftService::find(const VersionId& id) const {
    std::lock_guard lock(mutex_);
    for (const auto& v : snapshot_.versions) {
        if (v.id == id) {
            return v;
        }
    }
    return std::nullopt;
}

std::optional<VersionInfo> MinecraftService::deployedFor(VersionChannel channel) const {
    std::lock_guard lock(mutex_);
    for (const auto& v : snapshot_.versions) {
        if (v.deployed && v.id.channel == channel) {
            return v;
        }
    }
    return std::nullopt;
}

void MinecraftService::notify() {
    bool schedule = false;
    {
        std::lock_guard lock(mutex_);
        if (!notifyPending_) {
            notifyPending_ = true;
            schedule = true;
        }
    }
    if (!schedule) {
        return;
    }
    dispatcher_.post([this] {
        std::function<void()> handler;
        {
            std::lock_guard lock(mutex_);
            notifyPending_ = false;
            handler = changeHandler_;
        }
        if (handler) {
            handler();
        }
    });
}

void MinecraftService::rebuild() {
    std::vector<VersionInfo> out;
    auto find = [&out](const VersionId& id) -> VersionInfo& {
        for (auto& v : out) {
            if (v.id == id) {
                return v;
            }
        }
        out.push_back(VersionInfo{});
        out.back().id = id;
        return out.back();
    };
    for (const auto& entry : catalog_.entries) {
        find(entry.id).catalog = entry;
    }
    for (const auto& package : packages_) {
        auto& v = find(package.id);
        v.packageFile = package.path;
        v.packageSize = package.size;
    }
    for (const auto& partial : partials_) {
        find(partial.id).partialSize = partial.size;
    }
    for (const auto& deployed : deployed_) {
        auto& v = find(deployed.id);
        v.deployed = true;
        v.deployedByCitron = deployed.byCitron;
        v.deployedFullName = deployed.package.fullName;
        v.deployedLocation = deployed.package.installLocation;
    }
    std::ranges::stable_sort(out, [](const VersionInfo& a, const VersionInfo& b) {
        if (a.id.number != b.id.number) {
            return a.id.number > b.id.number;
        }
        return a.id.channel < b.id.channel;
    });
    snapshot_.versions = std::move(out);
    snapshot_.catalogUpdated = catalog_.updated;
}

void MinecraftService::start(std::string_view catalogUrl) {
    {
        std::lock_guard lock(mutex_);
        catalog_ = versions_.loadCatalog(embeddedCatalog_);
        packages_ = versions_.scanPackages();
        partials_ = versions_.scanPartials();
        rebuild();
        log::info("catalog ready: {} versions, {} packages on disk, {} partial downloads", catalog_.entries.size(), packages_.size(), partials_.size());
    }
    notify();
    refreshInstalled();
    refreshCatalog(catalogUrl);
    checkEnvironment();
}

void MinecraftService::refreshInstalled() {
    scheduler_.run([this](std::stop_token) {
        platform::initializeApartment();
        auto packages = versions_.scanPackages();
        auto partials = versions_.scanPartials();
        auto deployed = versions_.scanDeployed();
        for (const auto& d : deployed) {
            log::info("deployed {} at {} ({})", d.id.key(), d.package.installLocation.string(), d.byCitron ? "citron" : "other");
        }
        {
            std::lock_guard lock(mutex_);
            packages_ = std::move(packages);
            partials_ = std::move(partials);
            deployed_ = std::move(deployed);
            rebuild();
        }
        notify();
    });
}

void MinecraftService::refreshCatalog(std::string_view catalogUrl) {
    {
        std::lock_guard lock(mutex_);
        if (snapshot_.catalogLoading) {
            return;
        }
        snapshot_.catalogLoading = true;
        snapshot_.catalogError.reset();
    }
    notify();
    scheduler_.run([this, url = std::string(catalogUrl)](std::stop_token token) {
        const auto started = log::elapsedMs();
        auto fetched = versions_.fetchCatalog(url, token);
        {
            std::lock_guard lock(mutex_);
            snapshot_.catalogLoading = false;
            if (fetched) {
                catalog_ = std::move(*fetched);
                log::info("catalog refreshed: {} versions in {:.0f} ms", catalog_.entries.size(), log::elapsedMs() - started);
            } else {
                snapshot_.catalogError = fetched.error();
                log::warn("catalog refresh failed: {}", fetched.error().summary());
            }
            rebuild();
        }
        notify();
    });
}

void MinecraftService::checkEnvironment() {
    prerequisites_.check([this](platform::GdkEnvironment env) {
        {
            std::lock_guard lock(mutex_);
            snapshot_.environment = std::move(env);
        }
        notify();
    });
}

void MinecraftService::updateOperation(const VersionId& id, const InstallProgress& progress) {
    bool rescan = false;
    {
        std::lock_guard lock(mutex_);
        if (progress.stage == InstallStage::Cancelled) {
            snapshot_.operations.erase(id);
        } else {
            snapshot_.operations[id] = progress;
        }
        rescan = progress.stage == InstallStage::Completed || progress.stage == InstallStage::Cancelled || progress.stage == InstallStage::Failed;
    }
    if (rescan) {
        refreshInstalled();
    }
    notify();
}

void MinecraftService::install(const VersionId& id) {
    std::optional<CatalogEntry> entry;
    std::vector<std::string> urls;
    {
        std::lock_guard lock(mutex_);
        if (const auto* found = catalog_.find(id)) {
            entry = *found;
            urls = mirrorUrls(catalog_, *found);
        }
    }
    if (!entry) {
        InstallProgress p;
        p.stage = InstallStage::Failed;
        p.error = Error::make(ErrorCategory::VersionMetadata, "install", "This version is not in the version list.");
        updateOperation(id, p);
        return;
    }
    log::info("install requested for {}", id.key());
    installs_.install(id, *entry, std::move(urls));
}

void MinecraftService::cancel(const VersionId& id) {
    installs_.cancel(id);
}

void MinecraftService::dismiss(const VersionId& id) {
    {
        std::lock_guard lock(mutex_);
        auto it = snapshot_.operations.find(id);
        if (it != snapshot_.operations.end() && isTerminal(it->second.stage)) {
            snapshot_.operations.erase(it);
        }
    }
    notify();
}

bool MinecraftService::busy(const VersionId& id) const {
    return installs_.busy(id);
}

bool MinecraftService::anyBusy() const {
    return installs_.anyBusy() || launcher_.busy();
}

void MinecraftService::remove(const VersionId& id, std::function<void(Result<void>)> done) {
    auto info = find(id);
    if (!info) {
        done(std::unexpected(Error::make(ErrorCategory::Internal, "remove", "This version is not known.")));
        return;
    }
    if (busy(id)) {
        done(std::unexpected(Error::make(ErrorCategory::Internal, "remove", "This version is busy.")));
        return;
    }
    if (info->deployed && LaunchManager::isGameRunning()) {
        done(std::unexpected(Error::make(ErrorCategory::Launch, "remove", "Minecraft is running. Close it before removing this version.")));
        return;
    }
    log::info("remove requested for {}", id.key());
    if (info->packageFile) {
        if (!pathsafety::isInside(versions_.layout().installers, *info->packageFile)) {
            done(std::unexpected(Error::make(ErrorCategory::Filesystem, "remove", "The package is outside the launcher folder and was not touched.")));
            return;
        }
        if (auto removed = platform::removeFile(*info->packageFile); !removed) {
            done(std::unexpected(removed.error()));
            return;
        }
    }
    const auto partial = versions_.partialPath(id);
    if (auto cleared = platform::removeFile(partial); !cleared) {
        log::warn("partial download could not be removed: {}", cleared.error().summary());
    }
    removeDownloadMeta(partial);
    {
        std::lock_guard lock(mutex_);
        snapshot_.operations.erase(id);
    }
    if (info->deployed) {
        installs_.removeDeployment(id, info->deployedFullName, [this, done](const VersionId&, Result<void> result) {
            refreshInstalled();
            dispatcher_.post([done, result] { done(result); });
        });
        return;
    }
    refreshInstalled();
    done({});
}

void MinecraftService::activate(const VersionId& id, bool keepPackage, std::function<void(Result<std::filesystem::path>)> done) {
    auto info = find(id);
    if (!info || (!info->packageFile && !info->deployed)) {
        done(std::unexpected(Error::make(ErrorCategory::Package, "activate", "This version is not installed.")));
        return;
    }
    if (info->deployed) {
        done(info->deployedLocation);
        return;
    }
    if (busy(id)) {
        done(std::unexpected(Error::make(ErrorCategory::Internal, "activate", "This version is busy.")));
        return;
    }
    if (LaunchManager::isGameRunning()) {
        done(std::unexpected(Error::make(ErrorCategory::Launch, "activate", "Minecraft is running. Close it before switching versions.")));
        return;
    }
    std::optional<std::wstring> replace;
    if (auto current = deployedFor(id.channel)) {
        replace = current->deployedFullName;
    }
    const std::filesystem::path package = *info->packageFile;
    installs_.activate(id, package, replace, [this, done, keepPackage, package](const VersionId& vid, Result<platform::InstalledPackage> result) {
        if (result && !keepPackage) {
            log::info("discarding package file for {}", vid.key());
            if (auto removed = platform::removeFile(package); !removed) {
                log::warn("package file could not be removed: {}", removed.error().summary());
            }
        }
        refreshInstalled();
        Result<std::filesystem::path> location = result ? Result<std::filesystem::path>(result->installLocation) : std::unexpected(result.error());
        dispatcher_.post([done, location] { done(location); });
    });
}

void MinecraftService::reportLaunch(std::function<void(Result<void>)> done, Result<void> result) {
    {
        std::lock_guard lock(mutex_);
        snapshot_.launching = false;
    }
    notify();
    dispatcher_.post([done, result] { done(result); });
}

void MinecraftService::launch(const VersionId& id, bool keepPackage, std::function<void(Result<void>)> done) {
    auto info = find(id);
    if (!info) {
        done(std::unexpected(Error::make(ErrorCategory::Launch, "launch", "No version is selected.")));
        return;
    }
    if (!info->installed()) {
        done(std::unexpected(Error::make(ErrorCategory::Launch, "launch", "The selected version is not installed.")));
        return;
    }
    {
        std::lock_guard lock(mutex_);
        if (snapshot_.launching) {
            return;
        }
        if (snapshot_.environment && !snapshot_.environment->gamingServices) {
            done(std::unexpected(Error::make(ErrorCategory::Prerequisite, "launch", "Gaming Services is not installed.",
                                             "Minecraft needs the Gaming Services app from the Microsoft Store.")));
            return;
        }
        snapshot_.launching = true;
    }
    notify();
    LaunchMode mode = LaunchMode::Direct;
    {
        std::lock_guard lock(mutex_);
        if (snapshot_.environment && !snapshot_.environment->gameInput) {
            mode = LaunchMode::Helper;
        }
    }
    auto startGame = [this, id, mode, done](const std::filesystem::path& location) {
        launcher_.launch(id.channel, location, mode, [this, done](Result<void> result) { reportLaunch(done, std::move(result)); });
    };
    if (info->deployed) {
        startGame(info->deployedLocation);
        return;
    }
    activate(id, keepPackage, [this, done, startGame](Result<std::filesystem::path> result) {
        if (!result) {
            reportLaunch(done, std::unexpected(result.error()));
            return;
        }
        startGame(*result);
    });
}

Result<void> MinecraftService::setRoot(const paths::Layout& layout) {
    if (anyBusy()) {
        return std::unexpected(Error::make(ErrorCategory::Configuration, "set root", "Wait for running downloads to finish before changing the folder."));
    }
    if (auto ensured = paths::ensureDirectories(layout); !ensured) {
        return std::unexpected(ensured.error());
    }
    if (!platform::isWritableDirectory(layout.root)) {
        return std::unexpected(Error::make(ErrorCategory::Configuration, "set root", "The folder is not writable.", layout.root.string()));
    }
    {
        std::lock_guard lock(mutex_);
        versions_.setLayout(layout);
        snapshot_.operations.clear();
    }
    refreshInstalled();
    return {};
}

}

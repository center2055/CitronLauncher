#pragma once

#include "core/Error.h"
#include "core/Paths.h"
#include "minecraft/Version.h"
#include "minecraft/VersionCatalog.h"
#include "platform/windows/PackageManager.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace citron {

struct PackageFile {
    VersionId id;
    std::filesystem::path path;
    std::uint64_t size = 0;
};

struct DeployedPackage {
    VersionId id;
    platform::InstalledPackage package;
    bool byCitron = false;
};

struct PartialDownload {
    VersionId id;
    std::filesystem::path path;
    std::uint64_t size = 0;
};

struct DeployRecord {
    std::string fullName;
    std::string version;
};

class VersionManager {
public:
    explicit VersionManager(paths::Layout layout);

    void setLayout(paths::Layout layout);
    const paths::Layout& layout() const { return layout_; }

    std::vector<PackageFile> scanPackages() const;
    std::vector<PartialDownload> scanPartials() const;
    std::vector<DeployedPackage> scanDeployed() const;

    std::filesystem::path packagePath(const VersionId& id) const;
    std::filesystem::path partialPath(const VersionId& id) const;

    Catalog loadCatalog(std::string_view embedded) const;
    Result<Catalog> fetchCatalog(std::string_view url, std::stop_token token) const;

    std::map<std::string, DeployRecord> loadDeployRecords() const;
    void saveDeployRecord(VersionChannel channel, const DeployRecord& record) const;
    void clearDeployRecord(VersionChannel channel) const;

private:
    std::filesystem::path catalogCachePath() const;
    std::filesystem::path deployStatePath() const;

    paths::Layout layout_;
};

}

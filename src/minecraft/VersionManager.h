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
};

struct PartialDownload {
    VersionId id;
    std::filesystem::path path;
    std::uint64_t size = 0;
};

struct ManagedInstallation {
    VersionId id;
    std::filesystem::path path;
    std::uint64_t size = 0;
};

class VersionManager {
public:
    explicit VersionManager(paths::Layout layout);

    void setLayout(paths::Layout layout);
    const paths::Layout& layout() const { return layout_; }

    std::vector<PackageFile> scanPackages() const;
    std::vector<PartialDownload> scanPartials() const;
    std::vector<ManagedInstallation> scanManagedInstallations() const;
    std::vector<DeployedPackage> scanDeployed() const;

    std::filesystem::path packagePath(const VersionId& id) const;
    std::filesystem::path partialPath(const VersionId& id) const;
    std::filesystem::path managedPath(const VersionId& id) const;
    std::filesystem::path stagingPath(const VersionId& id) const;
    bool isCompleteManagedInstallation(const std::filesystem::path& directory) const;
    Result<void> prepareManagedInstallation(const VersionId& id, const std::filesystem::path& directory, std::stop_token token) const;
    Result<void> writeManagedMetadata(const VersionId& id, const std::filesystem::path& directory, std::string_view packageMd5) const;
    void cleanStaging() const;

    Catalog loadCatalog(std::string_view embedded) const;
    Result<Catalog> fetchCatalog(std::string_view url, std::stop_token token) const;

private:
    std::filesystem::path catalogCachePath() const;

    paths::Layout layout_;
};

}

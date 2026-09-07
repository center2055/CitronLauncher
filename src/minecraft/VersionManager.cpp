#include "minecraft/VersionManager.h"

#include "core/Json.h"
#include "core/Logger.h"
#include "core/PathSafety.h"
#include "core/Settings.h"
#include "core/Text.h"
#include "download/Http.h"
#include "platform/windows/FileOps.h"

namespace citron {

namespace {

constexpr std::uint64_t kMaxCatalogBytes = 4 * 1024 * 1024;

std::optional<VersionId> idFromFileName(const std::filesystem::path& file, std::string_view suffix) {
    const std::string name = text::toUtf8(file.filename().wstring());
    if (!text::endsWith(text::lower(name), suffix)) {
        return std::nullopt;
    }
    return versionFromPackageFullName(name.substr(0, name.size() - suffix.size()));
}

}

VersionManager::VersionManager(paths::Layout layout) : layout_(std::move(layout)) {}

void VersionManager::setLayout(paths::Layout layout) {
    layout_ = std::move(layout);
}

std::vector<PackageFile> VersionManager::scanPackages() const {
    std::vector<PackageFile> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(layout_.installers, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto id = idFromFileName(entry.path(), ".msixvc");
        if (!id) {
            continue;
        }
        const auto size = platform::fileSize(entry.path());
        if (!size || *size == 0) {
            continue;
        }
        out.push_back({*id, entry.path(), *size});
    }
    return out;
}

std::vector<PartialDownload> VersionManager::scanPartials() const {
    std::vector<PartialDownload> out;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(layout_.downloads, ec)) {
        if (!entry.is_regular_file(ec)) {
            continue;
        }
        const auto id = idFromFileName(entry.path(), ".msixvc.partial");
        if (!id) {
            continue;
        }
        out.push_back({*id, entry.path(), platform::fileSize(entry.path()).value_or(0)});
    }
    return out;
}

std::vector<DeployedPackage> VersionManager::scanDeployed() const {
    std::vector<DeployedPackage> out;
    const auto records = loadDeployRecords();
    for (const auto channel : {VersionChannel::Release, VersionChannel::Preview}) {
        for (const auto& package : platform::findPackagesByFamily(text::toWide(packageFamilyName(channel)))) {
            const auto id = versionFromPackageFullName(text::toUtf8(package.fullName));
            if (!id) {
                continue;
            }
            DeployedPackage deployed;
            deployed.id = *id;
            deployed.package = package;
            if (auto it = records.find(std::string(channelName(channel))); it != records.end()) {
                deployed.byCitron = text::equalsIgnoreCase(it->second.fullName, text::toUtf8(package.fullName));
            }
            out.push_back(std::move(deployed));
        }
    }
    return out;
}

std::filesystem::path VersionManager::packagePath(const VersionId& id) const {
    return layout_.installers / text::toWide(installerFileName(id));
}

std::filesystem::path VersionManager::partialPath(const VersionId& id) const {
    return layout_.downloads / text::toWide(installerFileName(id) + ".partial");
}

std::filesystem::path VersionManager::catalogCachePath() const {
    return layout_.cache / L"catalog.json";
}

std::filesystem::path VersionManager::deployStatePath() const {
    return layout_.cache / L"deployed.json";
}

Catalog VersionManager::loadCatalog(std::string_view embedded) const {
    Catalog best;
    if (auto parsed = parseCatalog(embedded)) {
        best = std::move(*parsed);
    } else {
        log::error("embedded catalog is invalid: {}", parsed.error().summary());
    }
    if (auto cached = readFile(catalogCachePath())) {
        if (auto parsed = parseCatalog(*cached)) {
            if (parsed->updated >= best.updated) {
                log::info("catalog: using cached list from {}", parsed->updated);
                return *parsed;
            }
        } else {
            log::warn("catalog cache is invalid: {}", parsed.error().summary());
        }
    }
    return best;
}

Result<Catalog> VersionManager::fetchCatalog(std::string_view url, std::stop_token token) const {
    auto body = http::get(url, kMaxCatalogBytes, token);
    if (!body) {
        return std::unexpected(body.error());
    }
    auto parsed = parseCatalog(*body);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    if (auto written = writeFileAtomically(catalogCachePath(), serializeCatalog(*parsed)); !written) {
        log::warn("catalog cache could not be written: {}", written.error().summary());
    }
    return parsed;
}

std::map<std::string, DeployRecord> VersionManager::loadDeployRecords() const {
    std::map<std::string, DeployRecord> out;
    auto content = readFile(deployStatePath());
    if (!content) {
        return out;
    }
    auto parsed = json::parse(*content);
    if (!parsed || !parsed->isObject()) {
        return out;
    }
    for (const auto& [channel, value] : parsed->asObject()) {
        DeployRecord record;
        record.fullName = value["fullName"].asString();
        record.version = value["version"].asString();
        if (!record.fullName.empty()) {
            out[channel] = record;
        }
    }
    return out;
}

void VersionManager::saveDeployRecord(VersionChannel channel, const DeployRecord& record) const {
    auto records = loadDeployRecords();
    records[std::string(channelName(channel))] = record;
    json::Value root = json::Object{};
    for (const auto& [name, rec] : records) {
        json::Value item = json::Object{};
        item.set("fullName", rec.fullName);
        item.set("version", rec.version);
        root.set(name, std::move(item));
    }
    if (auto written = writeFileAtomically(deployStatePath(), json::serialize(root)); !written) {
        log::warn("deploy state could not be written: {}", written.error().summary());
    }
}

void VersionManager::clearDeployRecord(VersionChannel channel) const {
    auto records = loadDeployRecords();
    records.erase(std::string(channelName(channel)));
    json::Value root = json::Object{};
    for (const auto& [name, rec] : records) {
        json::Value item = json::Object{};
        item.set("fullName", rec.fullName);
        item.set("version", rec.version);
        root.set(name, std::move(item));
    }
    if (auto written = writeFileAtomically(deployStatePath(), json::serialize(root)); !written) {
        log::warn("deploy state could not be written: {}", written.error().summary());
    }
}

}

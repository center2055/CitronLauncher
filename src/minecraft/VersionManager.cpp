#include "minecraft/VersionManager.h"

#include "core/Json.h"
#include "core/Logger.h"
#include "core/PathSafety.h"
#include "core/Settings.h"
#include "core/Text.h"
#include "download/Http.h"
#include "platform/windows/FileOps.h"

#include <windows.h>

#include <algorithm>
#include <format>
#include <regex>

namespace citron {

namespace {

constexpr std::uint64_t kMaxCatalogBytes = 4 * 1024 * 1024;
constexpr wchar_t kInstallMetadataFile[] = L"citron-install.json";
constexpr wchar_t kGdkManifestFile[] = L"AppxManifest.xml";

constexpr std::wstring_view kGdkRuntimeFiles[] = {
    L"vcruntime140_1.dll",
    L"concrt140_app.dll",
    L"msvcp140_app.dll",
    L"vcruntime140_app.dll",
};

std::optional<VersionId> idFromFileName(const std::filesystem::path& file, std::string_view suffix) {
    const std::string name = text::toUtf8(file.filename().wstring());
    if (!text::endsWith(text::lower(name), suffix)) {
        return std::nullopt;
    }
    return versionFromPackageFullName(name.substr(0, name.size() - suffix.size()));
}

std::filesystem::path versionDirectory(const paths::Layout& layout, const VersionId& id) {
    return layout.versions / text::toWide(channelName(id.channel)) / text::toWide(id.number.toString());
}

std::uint64_t directorySize(const std::filesystem::path& directory) {
    std::uint64_t total = 0;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec)) {
            total += platform::fileSize(entry.path()).value_or(0);
        }
    }
    return total;
}

// returns the recorded install size when the metadata is valid, or nullopt otherwise.
// the size may be 0 for installs written before the size was recorded.
std::optional<std::uint64_t> readValidMetadata(const std::filesystem::path& directory, const VersionId& id) {
    const auto content = readFile(directory / kInstallMetadataFile);
    if (!content) return std::nullopt;
    const auto parsed = json::parse(*content);
    if (!parsed || !parsed->isObject() || (*parsed)["id"].asString() != id.key() || (*parsed)["schema"].asInt() != 1) {
        return std::nullopt;
    }
    return (*parsed)["size"].asUnsigned(0);
}

bool hasCompleteGameFiles(const std::filesystem::path& directory) {
    const std::vector<std::wstring_view> files = {
        L"AppxManifest.xml", L"Minecraft.Windows.exe", L"MicrosoftGame.Config",
        L"vcruntime140_1.dll", L"concrt140_app.dll", L"msvcp140_app.dll", L"vcruntime140_app.dll",
    };
    for (const auto file : files) {
        if (!platform::fileExists(directory / file)) return false;
    }
    std::error_code ec;
    return std::filesystem::is_directory(directory / L"data", ec);
}

bool hasExtractedGameFiles(const std::filesystem::path& directory) {
    if (!platform::fileExists(directory / L"Minecraft.Windows.exe") || !platform::fileExists(directory / L"MicrosoftGame.Config")) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_directory(directory / L"data", ec);
}

std::string xmlEscape(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char c : value) {
        switch (c) {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '\"': escaped += "&quot;"; break;
        case '\'': escaped += "&apos;"; break;
        default: escaped += c; break;
        }
    }
    return escaped;
}

std::string attributeInElement(std::string_view xml, std::string_view element, std::string_view attribute, std::string fallback = {}) {
    const std::regex elementExpression("<" + std::string(element) + R"(\b[^>]*>)", std::regex::icase);
    const std::regex attributeExpression(std::string(attribute) + R"attr(\s*=\s*"([^"]+)")attr", std::regex::icase);
    const std::string content(xml);
    for (auto it = std::sregex_iterator(content.begin(), content.end(), elementExpression); it != std::sregex_iterator(); ++it) {
        std::smatch match;
        const std::string tag = it->str();
        if (std::regex_search(tag, match, attributeExpression) && match.size() >= 2) {
            return match[1].str();
        }
    }
    return fallback;
}

std::vector<std::filesystem::path> gdkRuntimeSources(const paths::Layout& layout) {
    std::vector<std::filesystem::path> out;
    out.push_back(layout.root / L"native" / L"launchercore");
    for (const auto channel : {VersionChannel::Release, VersionChannel::Preview}) {
        for (const auto& package : platform::findPackagesByFamily(text::toWide(packageFamilyName(channel)))) {
            if (!package.installLocation.empty()) out.push_back(package.installLocation);
        }
    }
    for (const auto name : {L"Microsoft.VCLibs.140.00.UWPDesktop", L"Microsoft.VCLibs.140.00"}) {
        for (const auto& package : platform::findPackagesByName(name)) {
            if (!package.installLocation.empty()) out.push_back(package.installLocation);
        }
    }
    std::ranges::sort(out);
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

Result<void> copyRuntimeFile(const std::filesystem::path& destination, const std::vector<std::filesystem::path>& sources) {
    if (platform::fileExists(destination)) {
        return {};
    }
    unsigned long lastError = ERROR_FILE_NOT_FOUND;
    for (const auto& sourceDirectory : sources) {
        const auto source = sourceDirectory / destination.filename();
        if (!platform::fileExists(source)) continue;
        if (CopyFileW(source.c_str(), destination.c_str(), TRUE)) {
            return {};
        }
        lastError = GetLastError();
    }
    return std::unexpected(Error::fromWin32(ErrorCategory::Prerequisite, "provision GDK runtime", lastError,
        "A required GDK runtime file could not be provisioned: " + destination.filename().string()));
}

Result<void> createLauncherManifest(const VersionId& id, const std::filesystem::path& directory) {
    const auto manifest = directory / kGdkManifestFile;
    if (platform::fileExists(manifest)) return {};
    const auto config = readFile(directory / L"MicrosoftGame.Config");
    if (!config) {
        return std::unexpected(Error::make(ErrorCategory::Package, "create GDK manifest", "MicrosoftGame.Config could not be read.",
                                           directory.string(), true));
    }
    const std::string identityName = attributeInElement(*config, "Identity", "Name", std::string(packageName(id.channel)));
    const std::string publisher = attributeInElement(*config, "Identity", "Publisher", "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US");
    const std::string version = attributeInElement(*config, "Identity", "Version", id.number.toPackageVersion());
    const std::string applicationId = attributeInElement(*config, "Executable", "Id", "App");
    const std::string executable = attributeInElement(*config, "Executable", "Name", "Minecraft.Windows.exe");
    const std::string displayName = attributeInElement(*config, "ShellVisuals", "DefaultDisplayName", "Minecraft for Windows");
    const std::string publisherDisplayName = attributeInElement(*config, "ShellVisuals", "PublisherDisplayName", "Microsoft Studios");
    const std::string description = attributeInElement(*config, "ShellVisuals", "Description", displayName);
    const std::string logo150 = attributeInElement(*config, "ShellVisuals", "Square150x150Logo", "Logo.png");
    const std::string logo44 = attributeInElement(*config, "ShellVisuals", "Square44x44Logo", "SmallLogo.png");
    const std::string splash = attributeInElement(*config, "ShellVisuals", "SplashScreenImage", "MCSplashScreen.png");
    const std::string foreground = attributeInElement(*config, "ShellVisuals", "ForegroundText", "light");
    const std::string background = attributeInElement(*config, "ShellVisuals", "BackgroundColor", "transparent");
    const std::string output = std::format(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<Package xmlns=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10\" xmlns:uap=\"http://schemas.microsoft.com/appx/manifest/uap/windows10\" xmlns:rescap=\"http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities\" xmlns:desktop6=\"http://schemas.microsoft.com/appx/manifest/desktop/windows10/6\" IgnorableNamespaces=\"uap rescap desktop6\">\n"
        "  <Identity Name=\"{}\" Publisher=\"{}\" Version=\"{}\" ProcessorArchitecture=\"x64\" />\n"
        "  <Properties><DisplayName>{}</DisplayName><PublisherDisplayName>{}</PublisherDisplayName><Logo>StoreLogo.png</Logo><Description>{}</Description><desktop6:RegistryWriteVirtualization>disabled</desktop6:RegistryWriteVirtualization><desktop6:FileSystemWriteVirtualization>disabled</desktop6:FileSystemWriteVirtualization></Properties>\n"
        "  <Dependencies><TargetDeviceFamily Name=\"Windows.Desktop\" MinVersion=\"10.0.18362.0\" MaxVersionTested=\"10.0.18362.0\" /><PackageDependency Name=\"Microsoft.VCLibs.140.00.UWPDesktop\" MinVersion=\"14.0.33728.0\" Publisher=\"CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US\" /></Dependencies>\n"
        "  <Resources><Resource Language=\"en-us\" /></Resources>\n"
        "  <Applications><Application Id=\"{}\" Executable=\"{}\" EntryPoint=\"Windows.FullTrustApplication\"><uap:VisualElements DisplayName=\"{}\" Square150x150Logo=\"{}\" Square44x44Logo=\"{}\" Description=\"{}\" ForegroundText=\"{}\" BackgroundColor=\"{}\"><uap:SplashScreen Image=\"{}\" /></uap:VisualElements></Application></Applications>\n"
        "  <Capabilities><Capability Name=\"internetClient\" /><rescap:Capability Name=\"runFullTrust\" /><rescap:Capability Name=\"appLicensing\" /><rescap:Capability Name=\"unvirtualizedResources\" /></Capabilities>\n"
        "</Package>\n",
        xmlEscape(identityName), xmlEscape(publisher), xmlEscape(version), xmlEscape(displayName), xmlEscape(publisherDisplayName), xmlEscape(description),
        xmlEscape(applicationId), xmlEscape(executable), xmlEscape(displayName), xmlEscape(logo150), xmlEscape(logo44), xmlEscape(description),
        xmlEscape(foreground), xmlEscape(background), xmlEscape(splash));
    return writeFileAtomically(manifest, output);
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

std::vector<ManagedInstallation> VersionManager::scanManagedInstallations() const {
    std::vector<ManagedInstallation> out;
    for (const auto channel : {VersionChannel::Release, VersionChannel::Preview}) {
        std::error_code ec;
        const auto channelDirectory = layout_.versions / text::toWide(channelName(channel));
        for (const auto& entry : std::filesystem::directory_iterator(channelDirectory, ec)) {
            if (!entry.is_directory(ec)) continue;
            const std::string number = text::toUtf8(entry.path().filename().wstring());
            const auto parsed = VersionNumber::parse(number);
            if (!parsed) continue;
            const VersionId id{channel, *parsed};
            const auto recordedSize = readValidMetadata(entry.path(), id);
            if (!recordedSize || !hasCompleteGameFiles(entry.path())) continue;
            out.push_back({id, entry.path(), *recordedSize});
        }
    }
    return out;
}

std::vector<DeployedPackage> VersionManager::scanDeployed() const {
    std::vector<DeployedPackage> out;
    for (const auto channel : {VersionChannel::Release, VersionChannel::Preview}) {
        for (const auto& package : platform::findPackagesByFamily(text::toWide(packageFamilyName(channel)))) {
            const auto id = versionFromPackageFullName(text::toUtf8(package.fullName));
            if (!id) {
                continue;
            }
            DeployedPackage deployed;
            deployed.id = *id;
            deployed.package = package;
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

std::filesystem::path VersionManager::managedPath(const VersionId& id) const {
    return versionDirectory(layout_, id);
}

std::filesystem::path VersionManager::stagingPath(const VersionId& id) const {
    return versionDirectory(layout_, id).wstring() + L".partial";
}

bool VersionManager::isCompleteManagedInstallation(const std::filesystem::path& directory) const {
    return pathsafety::isInside(layout_.versions, directory) && hasCompleteGameFiles(directory);
}

Result<void> VersionManager::prepareManagedInstallation(const VersionId& id, const std::filesystem::path& directory, std::stop_token token) const {
    if (!pathsafety::isInside(layout_.versions, directory)) {
        return std::unexpected(Error::make(ErrorCategory::Filesystem, "prepare GDK version", "The version directory is outside Citron's managed folder."));
    }
    if (!hasExtractedGameFiles(directory)) {
        return std::unexpected(Error::make(ErrorCategory::Package, "prepare GDK version", "The extractor did not produce the required game files.",
                                           "Expected Minecraft.Windows.exe, MicrosoftGame.Config, and the data directory.", true));
    }
    if (token.stop_requested()) {
        return std::unexpected(Error::cancelled("prepare GDK version"));
    }
    const auto sources = gdkRuntimeSources(layout_);
    if (sources.empty()) {
        return std::unexpected(Error::make(ErrorCategory::Prerequisite, "prepare GDK version", "The GDK runtime files could not be located.",
            "Install Minecraft for Windows or Microsoft Visual C++ UWP runtime before installing an isolated version.", true));
    }
    for (const auto file : kGdkRuntimeFiles) {
        if (token.stop_requested()) {
            return std::unexpected(Error::cancelled("prepare GDK version"));
        }
        if (auto copied = copyRuntimeFile(directory / file, sources); !copied) {
            return std::unexpected(copied.error());
        }
    }
    if (token.stop_requested()) {
        return std::unexpected(Error::cancelled("prepare GDK version"));
    }
    return createLauncherManifest(id, directory);
}

Result<void> VersionManager::writeManagedMetadata(const VersionId& id, const std::filesystem::path& directory, std::string_view packageMd5) const {
    if (!pathsafety::isInside(layout_.versions, directory)) {
        return std::unexpected(Error::make(ErrorCategory::Filesystem, "write version metadata", "The version directory is outside Citron's managed folder."));
    }
    json::Value value = json::Object{};
    value.set("schema", 1);
    value.set("id", id.key());
    value.set("package", installerFileName(id));
    value.set("md5", packageMd5);
    value.set("size", directorySize(directory));
    value.set("installedAt", platform::unixNow());
    return writeFileAtomically(directory / kInstallMetadataFile, json::serialize(value));
}

void VersionManager::cleanStaging() const {
    for (const auto channel : {VersionChannel::Release, VersionChannel::Preview}) {
        std::error_code ec;
        const auto channelDirectory = layout_.versions / text::toWide(channelName(channel));
        for (const auto& entry : std::filesystem::directory_iterator(channelDirectory, ec)) {
            if (!entry.is_directory(ec) || !text::endsWith(text::toUtf8(entry.path().filename().wstring()), ".partial")) continue;
            if (pathsafety::isInside(layout_.versions, entry.path())) {
                if (auto removed = platform::removeDirectoryTree(entry.path()); !removed) {
                    log::warn("could not clean stale version staging directory {}: {}", entry.path().string(), removed.error().summary());
                }
            }
        }
    }
}

std::filesystem::path VersionManager::catalogCachePath() const {
    return layout_.cache / L"catalog.json";
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

}

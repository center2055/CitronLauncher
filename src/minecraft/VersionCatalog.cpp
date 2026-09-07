#include "minecraft/VersionCatalog.h"

#include "core/Json.h"
#include "core/Text.h"

#include <algorithm>

namespace citron {

namespace {

constexpr std::uint64_t kMaxPackageSize = 64ull * 1024 * 1024 * 1024;

bool isHex32(std::string_view s) {
    if (s.size() != 32) {
        return false;
    }
    return std::ranges::all_of(s, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool isSafeHost(std::string_view host) {
    if (host.empty() || host.size() > 253) {
        return false;
    }
    return std::ranges::all_of(host, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-'; });
}

bool isSafePath(std::string_view path) {
    if (path.empty() || path.size() > 1024 || path[0] != '/') {
        return false;
    }
    if (path.find("..") != std::string_view::npos || path.find("//") != std::string_view::npos) {
        return false;
    }
    return std::ranges::all_of(path, [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u > 0x20 && u < 0x7F && c != '\\' && c != '"' && c != '<' && c != '>' && c != '#' && c != '?';
    });
}

Error metadataError(std::string message) {
    return Error::make(ErrorCategory::VersionMetadata, "parse catalog", std::move(message));
}

}

const CatalogEntry* Catalog::find(const VersionId& id) const {
    for (const auto& entry : entries) {
        if (entry.id == id) {
            return &entry;
        }
    }
    return nullptr;
}

Result<Catalog> parseCatalog(std::string_view text) {
    auto parsed = json::parse(text);
    if (!parsed) {
        return std::unexpected(metadataError("The version list is not valid JSON: " + parsed.error()));
    }
    const json::Value& root = *parsed;
    if (!root.isObject()) {
        return std::unexpected(metadataError("The version list has an unexpected layout"));
    }
    Catalog catalog;
    catalog.format = static_cast<int>(root["format"].asInt(0));
    if (catalog.format != 1) {
        return std::unexpected(metadataError("The version list uses an unsupported format"));
    }
    catalog.updated = root["updated"].asInt(0);
    for (const auto& host : root["hosts"].asArray()) {
        const std::string h = text::lower(host.asString());
        if (isSafeHost(h)) {
            catalog.hosts.push_back(h);
        }
    }
    if (catalog.hosts.empty()) {
        return std::unexpected(metadataError("The version list has no download hosts"));
    }
    for (const auto& item : root["versions"].asArray()) {
        const auto channel = parseChannel(item["channel"].asString());
        const auto number = VersionNumber::parse(item["version"].asString());
        if (!channel || !number) {
            continue;
        }
        CatalogEntry entry;
        entry.id = VersionId{*channel, *number};
        entry.size = item["size"].asUnsigned(0);
        entry.md5 = text::lower(item["md5"].asString());
        entry.released = item["released"].asInt(0);
        entry.path = item["path"].asString();
        if (entry.size > kMaxPackageSize || !isHex32(entry.md5) || !isSafePath(entry.path)) {
            continue;
        }
        if (catalog.find(entry.id) != nullptr) {
            continue;
        }
        catalog.entries.push_back(std::move(entry));
    }
    sortNewestFirst(catalog.entries);
    return catalog;
}

std::string serializeCatalog(const Catalog& catalog) {
    json::Value root = json::Object{};
    root.set("format", catalog.format);
    root.set("updated", catalog.updated);
    json::Value hosts = json::Array{};
    for (const auto& host : catalog.hosts) {
        hosts.push(host);
    }
    root.set("hosts", std::move(hosts));
    json::Value versions = json::Array{};
    for (const auto& entry : catalog.entries) {
        json::Value item = json::Object{};
        item.set("version", entry.id.number.toString());
        item.set("channel", std::string(channelName(entry.id.channel)));
        item.set("released", entry.released);
        item.set("size", entry.size);
        item.set("md5", entry.md5);
        item.set("path", entry.path);
        versions.push(std::move(item));
    }
    root.set("versions", std::move(versions));
    return json::serialize(root);
}

std::vector<std::string> mirrorUrls(const Catalog& catalog, const CatalogEntry& entry) {
    std::vector<std::string> urls;
    urls.reserve(catalog.hosts.size());
    for (const auto& host : catalog.hosts) {
        urls.push_back("http://" + host + entry.path);
    }
    return urls;
}

void sortNewestFirst(std::vector<CatalogEntry>& entries) {
    std::ranges::stable_sort(entries, [](const CatalogEntry& a, const CatalogEntry& b) {
        if (a.id.number != b.id.number) {
            return a.id.number > b.id.number;
        }
        return a.id.channel < b.id.channel;
    });
}

}

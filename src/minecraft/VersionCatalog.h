#pragma once

#include "core/Error.h"
#include "minecraft/Version.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace citron {

struct CatalogEntry {
    VersionId id;
    std::uint64_t size = 0;
    std::string md5;
    std::int64_t released = 0;
    std::string path;
};

struct Catalog {
    int format = 0;
    std::int64_t updated = 0;
    std::vector<std::string> hosts;
    std::vector<CatalogEntry> entries;

    const CatalogEntry* find(const VersionId& id) const;
    bool empty() const { return entries.empty(); }
};

Result<Catalog> parseCatalog(std::string_view text);
std::string serializeCatalog(const Catalog& catalog);
std::vector<std::string> mirrorUrls(const Catalog& catalog, const CatalogEntry& entry);
void sortNewestFirst(std::vector<CatalogEntry>& entries);

}

#include "download/DownloadMeta.h"

#include "core/Settings.h"

namespace citron {

json::Value DownloadMeta::toJson() const {
    json::Value v = json::Object{};
    v.set("url", url);
    v.set("etag", etag);
    v.set("lastModified", lastModified);
    v.set("totalSize", totalSize);
    v.set("md5", md5);
    v.set("started", started);
    return v;
}

std::optional<DownloadMeta> DownloadMeta::fromJson(const json::Value& v) {
    if (!v.isObject()) {
        return std::nullopt;
    }
    DownloadMeta m;
    m.url = v["url"].asString();
    m.etag = v["etag"].asString();
    m.lastModified = v["lastModified"].asString();
    m.totalSize = v["totalSize"].asUnsigned(0);
    m.md5 = v["md5"].asString();
    m.started = v["started"].asInt(0);
    if (m.url.empty()) {
        return std::nullopt;
    }
    return m;
}

std::filesystem::path metaPathFor(const std::filesystem::path& partialFile) {
    return partialFile.wstring() + L".json";
}

std::optional<DownloadMeta> readDownloadMeta(const std::filesystem::path& partialFile) {
    auto content = readFile(metaPathFor(partialFile));
    if (!content) {
        return std::nullopt;
    }
    auto parsed = json::parse(*content);
    if (!parsed) {
        return std::nullopt;
    }
    return DownloadMeta::fromJson(*parsed);
}

Result<void> writeDownloadMeta(const std::filesystem::path& partialFile, const DownloadMeta& meta) {
    return writeFileAtomically(metaPathFor(partialFile), json::serialize(meta.toJson()));
}

void removeDownloadMeta(const std::filesystem::path& partialFile) {
    std::error_code ec;
    std::filesystem::remove(metaPathFor(partialFile), ec);
}

}

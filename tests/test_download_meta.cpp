#include "check.h"

#include "download/DownloadMeta.h"

#include <filesystem>

using namespace citron;

TEST_CASE(download_meta_json_round_trip) {
    DownloadMeta m;
    m.url = "http://assets1.xboxlive.com/Z/x.msixvc";
    m.etag = "\"0x8DF085C77577EDA\"";
    m.lastModified = "Tue, 01 Sep 2026 19:08:54 GMT";
    m.totalSize = 2142892032ull;
    m.md5 = "fb3e9b5bc32e1cda9a62b74cb903c226";
    m.started = 1788730454;
    auto back = DownloadMeta::fromJson(m.toJson());
    CHECK(back.has_value());
    CHECK_EQ(back->url, m.url);
    CHECK_EQ(back->etag, m.etag);
    CHECK_EQ(back->lastModified, m.lastModified);
    CHECK_EQ(back->totalSize, m.totalSize);
    CHECK_EQ(back->md5, m.md5);
    CHECK_EQ(back->started, m.started);
}

TEST_CASE(download_meta_rejects_missing_url) {
    CHECK(!DownloadMeta::fromJson(json::Value(json::Object{})).has_value());
    CHECK(!DownloadMeta::fromJson(json::Value(42)).has_value());
}

TEST_CASE(download_meta_file_round_trip) {
    const auto dir = std::filesystem::temp_directory_path() / "citron-tests";
    std::filesystem::create_directories(dir);
    const auto partial = dir / "pkg.msixvc.partial";
    DownloadMeta m;
    m.url = "http://host/path";
    m.totalSize = 10;
    m.md5 = "abc";
    CHECK(writeDownloadMeta(partial, m).has_value());
    CHECK_EQ(metaPathFor(partial), std::filesystem::path(dir / "pkg.msixvc.partial.json"));
    auto read = readDownloadMeta(partial);
    CHECK(read.has_value());
    CHECK_EQ(read->url, "http://host/path");
    removeDownloadMeta(partial);
    CHECK(!std::filesystem::exists(metaPathFor(partial)));
    CHECK(!readDownloadMeta(partial).has_value());
}

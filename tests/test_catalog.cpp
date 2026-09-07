#include "check.h"

#include "minecraft/VersionCatalog.h"

using namespace citron;

namespace {

const char* kSample = R"({
  "format": 1,
  "updated": 1788730454,
  "hosts": ["assets1.xboxlive.com", "assets2.xboxlive.com"],
  "versions": [
    {"version": "1.26.45.01", "channel": "release", "released": 10, "size": 2000, "md5": "D1A1C8526374596BE545B046DB3F6E47", "path": "/Z/a/b/Microsoft.MinecraftUWP_1.26.4501.0_x64__8wekyb3d8bbwe.msixvc"},
    {"version": "1.26.60.21", "channel": "preview", "released": 20, "size": 3000, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "/Z/c/d/Microsoft.MinecraftWindowsBeta_1.26.6021.0_x64__8wekyb3d8bbwe.msixvc"},
    {"version": "1.26.44.03", "channel": "release", "released": 5, "size": 1000, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "/Z/e/f/x.msixvc"},
    {"version": "bad", "channel": "release", "size": 1, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "/x"},
    {"version": "1.0.0.01", "channel": "release", "size": 1, "md5": "short", "path": "/x"},
    {"version": "1.0.0.02", "channel": "release", "size": 1, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "/../escape"},
    {"version": "1.0.0.03", "channel": "release", "size": 1, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "relative"},
    {"version": "1.26.45.01", "channel": "release", "size": 9, "md5": "fb3e9b5bc32e1cda9a62b74cb903c226", "path": "/dup"}
  ]
})";

}

TEST_CASE(catalog_parse_filters_and_sorts) {
    auto c = parseCatalog(kSample);
    CHECK(c.has_value());
    CHECK_EQ(c->entries.size(), 3u);
    CHECK_EQ(c->entries[0].id.number.toString(), "1.26.60.21");
    CHECK_EQ(c->entries[1].id.number.toString(), "1.26.45.01");
    CHECK_EQ(c->entries[2].id.number.toString(), "1.26.44.03");
    CHECK_EQ(c->entries[1].md5, "d1a1c8526374596be545b046db3f6e47");
    CHECK_EQ(c->entries[1].size, 2000ull);
    CHECK_EQ(c->hosts.size(), 2u);
}

TEST_CASE(catalog_urls_and_lookup) {
    auto c = parseCatalog(kSample);
    CHECK(c.has_value());
    VersionId id{VersionChannel::Preview, *VersionNumber::parse("1.26.60.21")};
    const auto* entry = c->find(id);
    CHECK(entry != nullptr);
    auto urls = mirrorUrls(*c, *entry);
    CHECK_EQ(urls.size(), 2u);
    CHECK_EQ(urls[0], "http://assets1.xboxlive.com/Z/c/d/Microsoft.MinecraftWindowsBeta_1.26.6021.0_x64__8wekyb3d8bbwe.msixvc");
    CHECK(c->find(VersionId{VersionChannel::Release, *VersionNumber::parse("9.9.9.09")}) == nullptr);
}

TEST_CASE(catalog_rejects_bad_documents) {
    CHECK(!parseCatalog("not json").has_value());
    CHECK(!parseCatalog("[]").has_value());
    CHECK(!parseCatalog(R"({"format": 2, "hosts": ["a"], "versions": []})").has_value());
    CHECK(!parseCatalog(R"({"format": 1, "hosts": [], "versions": []})").has_value());
    CHECK(!parseCatalog(R"({"format": 1, "hosts": ["bad host!"], "versions": []})").has_value());
}

TEST_CASE(catalog_serialize_round_trip) {
    auto c = parseCatalog(kSample);
    CHECK(c.has_value());
    auto again = parseCatalog(serializeCatalog(*c));
    CHECK(again.has_value());
    CHECK_EQ(again->entries.size(), c->entries.size());
    CHECK_EQ(again->updated, c->updated);
    CHECK(again->entries[0].id == c->entries[0].id);
    CHECK_EQ(again->entries[0].path, c->entries[0].path);
}

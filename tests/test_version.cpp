#include "check.h"

#include "minecraft/Version.h"

using namespace citron;

TEST_CASE(version_parse_display_form) {
    auto v = VersionNumber::parse("1.26.45.01");
    CHECK(v.has_value());
    CHECK_EQ(v->parts[0], 1u);
    CHECK_EQ(v->parts[1], 26u);
    CHECK_EQ(v->parts[2], 45u);
    CHECK_EQ(v->parts[3], 1u);
    CHECK_EQ(v->toString(), "1.26.45.01");
    CHECK_EQ(v->toPackageVersion(), "1.26.4501.0");
}

TEST_CASE(version_parse_rejects_garbage) {
    CHECK(!VersionNumber::parse("").has_value());
    CHECK(!VersionNumber::parse("1.26").has_value());
    CHECK(!VersionNumber::parse("1.26.45.01.2").has_value());
    CHECK(!VersionNumber::parse("a.b.c.d").has_value());
    CHECK(!VersionNumber::parse("1.26.45.100").has_value());
    CHECK(!VersionNumber::parse("1..2.3").has_value());
}

TEST_CASE(version_package_round_trip) {
    auto v = VersionNumber::fromPackageVersion("1.21.12021.0");
    CHECK(v.has_value());
    CHECK_EQ(v->toString(), "1.21.120.21");
    CHECK_EQ(v->toPackageVersion(), "1.21.12021.0");
    auto low = VersionNumber::fromPackageVersion("1.26.4.0");
    CHECK(low.has_value());
    CHECK_EQ(low->toString(), "1.26.0.04");
}

TEST_CASE(version_ordering) {
    auto a = *VersionNumber::parse("1.26.45.01");
    auto b = *VersionNumber::parse("1.26.50.27");
    auto c = *VersionNumber::parse("1.21.120.04");
    CHECK(a < b);
    CHECK(c < a);
    CHECK(a == *VersionNumber::parse("1.26.45.01"));
}

TEST_CASE(version_id_key) {
    VersionId id{VersionChannel::Preview, *VersionNumber::parse("1.26.60.21")};
    CHECK_EQ(id.key(), "preview/1.26.60.21");
    auto back = VersionId::parse("preview/1.26.60.21");
    CHECK(back.has_value());
    CHECK(*back == id);
    CHECK(!VersionId::parse("beta/1.0.0.0").has_value());
    CHECK(!VersionId::parse("release").has_value());
}

TEST_CASE(version_package_names) {
    VersionId release{VersionChannel::Release, *VersionNumber::parse("1.26.45.01")};
    CHECK_EQ(packageFullName(release), "Microsoft.MinecraftUWP_1.26.4501.0_x64__8wekyb3d8bbwe");
    CHECK_EQ(installerFileName(release), "Microsoft.MinecraftUWP_1.26.4501.0_x64__8wekyb3d8bbwe.msixvc");
    auto parsed = versionFromPackageFullName("MICROSOFT.MINECRAFTUWP_1.21.13201.0_x64__8wekyb3d8bbwe");
    CHECK(parsed.has_value());
    CHECK(parsed->channel == VersionChannel::Release);
    CHECK_EQ(parsed->number.toString(), "1.21.132.01");
    auto preview = versionFromPackageFullName("Microsoft.MinecraftWindowsBeta_1.26.5027.0_x64__8wekyb3d8bbwe");
    CHECK(preview.has_value());
    CHECK(preview->channel == VersionChannel::Preview);
    CHECK(!versionFromPackageFullName("Microsoft.GamingServices_38.116.6003.0_x64__8wekyb3d8bbwe").has_value());
}

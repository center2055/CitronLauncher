#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace citron {

enum class VersionChannel {
    Release,
    Preview,
};

std::string_view channelName(VersionChannel channel);
std::optional<VersionChannel> parseChannel(std::string_view text);

struct VersionNumber {
    std::array<std::uint32_t, 4> parts{};

    static std::optional<VersionNumber> parse(std::string_view text);
    static std::optional<VersionNumber> fromPackageVersion(std::string_view text);

    std::string toString() const;
    std::string toPackageVersion() const;

    auto operator<=>(const VersionNumber&) const = default;
};

struct VersionId {
    VersionChannel channel = VersionChannel::Release;
    VersionNumber number;

    std::string key() const;
    static std::optional<VersionId> parse(std::string_view key);

    auto operator<=>(const VersionId&) const = default;
};

std::string_view packageName(VersionChannel channel);
std::string_view packageFamilyName(VersionChannel channel);
std::string packageFullName(const VersionId& id);
std::string installerFileName(const VersionId& id);
std::optional<VersionId> versionFromPackageFullName(std::string_view fullName);
std::optional<VersionChannel> channelFromPackageName(std::string_view name);

}

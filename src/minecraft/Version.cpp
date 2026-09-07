#include "minecraft/Version.h"

#include "core/Text.h"

#include <charconv>
#include <format>
#include <vector>

namespace citron {

namespace {

constexpr std::string_view kReleaseName = "Microsoft.MinecraftUWP";
constexpr std::string_view kPreviewName = "Microsoft.MinecraftWindowsBeta";
constexpr std::string_view kPublisherId = "8wekyb3d8bbwe";

std::optional<std::array<std::uint32_t, 4>> parseParts(std::string_view text) {
    const auto pieces = text::split(text::trim(text), '.');
    if (pieces.size() != 4) {
        return std::nullopt;
    }
    std::array<std::uint32_t, 4> parts{};
    for (size_t i = 0; i < 4; ++i) {
        const auto& piece = pieces[i];
        if (piece.empty() || piece.size() > 9) {
            return std::nullopt;
        }
        std::uint32_t value = 0;
        const auto result = std::from_chars(piece.data(), piece.data() + piece.size(), value);
        if (result.ec != std::errc() || result.ptr != piece.data() + piece.size()) {
            return std::nullopt;
        }
        parts[i] = value;
    }
    return parts;
}

}

std::string_view channelName(VersionChannel channel) {
    return channel == VersionChannel::Preview ? "preview" : "release";
}

std::optional<VersionChannel> parseChannel(std::string_view text) {
    if (text::equalsIgnoreCase(text, "release")) {
        return VersionChannel::Release;
    }
    if (text::equalsIgnoreCase(text, "preview")) {
        return VersionChannel::Preview;
    }
    return std::nullopt;
}

std::optional<VersionNumber> VersionNumber::parse(std::string_view text) {
    auto parts = parseParts(text);
    if (!parts) {
        return std::nullopt;
    }
    if ((*parts)[3] > 99) {
        return std::nullopt;
    }
    return VersionNumber{*parts};
}

std::optional<VersionNumber> VersionNumber::fromPackageVersion(std::string_view text) {
    auto parts = parseParts(text);
    if (!parts) {
        return std::nullopt;
    }
    VersionNumber v;
    v.parts[0] = (*parts)[0];
    v.parts[1] = (*parts)[1];
    v.parts[2] = (*parts)[2] / 100;
    v.parts[3] = (*parts)[2] % 100;
    return v;
}

std::string VersionNumber::toString() const {
    return std::format("{}.{}.{}.{:02}", parts[0], parts[1], parts[2], parts[3]);
}

std::string VersionNumber::toPackageVersion() const {
    return std::format("{}.{}.{}.0", parts[0], parts[1], parts[2] * 100 + parts[3]);
}

std::string VersionId::key() const {
    return std::format("{}/{}", channelName(channel), number.toString());
}

std::optional<VersionId> VersionId::parse(std::string_view key) {
    const auto slash = key.find('/');
    if (slash == std::string_view::npos) {
        return std::nullopt;
    }
    const auto channel = parseChannel(key.substr(0, slash));
    const auto number = VersionNumber::parse(key.substr(slash + 1));
    if (!channel || !number) {
        return std::nullopt;
    }
    return VersionId{*channel, *number};
}

std::string_view packageName(VersionChannel channel) {
    return channel == VersionChannel::Preview ? kPreviewName : kReleaseName;
}

std::string_view packageFamilyName(VersionChannel channel) {
    return channel == VersionChannel::Preview ? "Microsoft.MinecraftWindowsBeta_8wekyb3d8bbwe" : "Microsoft.MinecraftUWP_8wekyb3d8bbwe";
}

std::string packageFullName(const VersionId& id) {
    return std::format("{}_{}_x64__{}", packageName(id.channel), id.number.toPackageVersion(), kPublisherId);
}

std::string installerFileName(const VersionId& id) {
    return packageFullName(id) + ".msixvc";
}

std::optional<VersionChannel> channelFromPackageName(std::string_view name) {
    if (text::equalsIgnoreCase(name, kReleaseName)) {
        return VersionChannel::Release;
    }
    if (text::equalsIgnoreCase(name, kPreviewName)) {
        return VersionChannel::Preview;
    }
    return std::nullopt;
}

std::optional<VersionId> versionFromPackageFullName(std::string_view fullName) {
    const auto pieces = text::split(fullName, '_');
    if (pieces.size() < 3) {
        return std::nullopt;
    }
    const auto channel = channelFromPackageName(pieces[0]);
    const auto number = VersionNumber::fromPackageVersion(pieces[1]);
    if (!channel || !number) {
        return std::nullopt;
    }
    return VersionId{*channel, *number};
}

}

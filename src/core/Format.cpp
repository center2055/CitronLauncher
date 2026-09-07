#include "core/Format.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace citron::format {

namespace {

constexpr double kKilo = 1024.0;
constexpr double kMega = kKilo * 1024.0;
constexpr double kGiga = kMega * 1024.0;

std::string scaled(double value, const char* suffix) {
    if (value >= kGiga) {
        return std::format("{:.2f} GB{}", value / kGiga, suffix);
    }
    if (value >= kMega) {
        return std::format("{:.1f} MB{}", value / kMega, suffix);
    }
    if (value >= kKilo) {
        return std::format("{:.0f} KB{}", value / kKilo, suffix);
    }
    return std::format("{:.0f} B{}", value, suffix);
}

}

std::string bytes(std::uint64_t value) {
    return scaled(static_cast<double>(value), "");
}

std::string speed(double bytesPerSecond) {
    if (!std::isfinite(bytesPerSecond) || bytesPerSecond < 0) {
        bytesPerSecond = 0;
    }
    return scaled(bytesPerSecond, "/s");
}

std::string eta(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0) {
        return "--:--";
    }
    const auto total = static_cast<std::int64_t>(std::llround(seconds));
    const auto hours = total / 3600;
    const auto minutes = (total % 3600) / 60;
    const auto secs = total % 60;
    if (hours > 0) {
        return std::format("{}:{:02}:{:02}", hours, minutes, secs);
    }
    return std::format("{}:{:02}", minutes, secs);
}

std::string percent(double fraction) {
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    return std::format("{:.0f} %", clamped * 100.0);
}

}

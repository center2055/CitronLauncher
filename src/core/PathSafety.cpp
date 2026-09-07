#include "core/PathSafety.h"

#include "core/Text.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>

namespace citron::pathsafety {

namespace {

constexpr std::array<std::string_view, 22> kReserved = {
    "con", "prn", "aux", "nul",
    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
};

std::wstring lowerWide(std::wstring s) {
    std::ranges::transform(s, s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

}

bool isSafeFileName(std::string_view name) {
    if (name.empty() || name.size() > 200) {
        return false;
    }
    if (name == "." || name == "..") {
        return false;
    }
    for (const char c : name) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            return false;
        }
    }
    if (name.back() == '.' || name.back() == ' ') {
        return false;
    }
    std::string stem = text::lower(name);
    if (const auto dot = stem.find('.'); dot != std::string::npos) {
        stem = stem.substr(0, dot);
    }
    return std::ranges::find(kReserved, stem) == kReserved.end();
}

std::filesystem::path normalize(const std::filesystem::path& path) {
    std::wstring s = path.lexically_normal().wstring();
    if (s.starts_with(L"\\\\?\\")) {
        s.erase(0, 4);
    }
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) {
        s.pop_back();
    }
    std::ranges::replace(s, L'/', L'\\');
    return std::filesystem::path(lowerWide(std::move(s)));
}

bool isInside(const std::filesystem::path& root, const std::filesystem::path& target) {
    const std::wstring r = normalize(root).wstring();
    const std::wstring t = normalize(target).wstring();
    if (r.empty() || t.size() < r.size()) {
        return false;
    }
    if (t.compare(0, r.size(), r) != 0) {
        return false;
    }
    return t.size() == r.size() || t[r.size()] == L'\\';
}

std::optional<std::filesystem::path> childOf(const std::filesystem::path& root, std::string_view name) {
    if (!isSafeFileName(name)) {
        return std::nullopt;
    }
    std::filesystem::path child = root / text::toWide(name);
    if (!isInside(root, child) || normalize(child) == normalize(root)) {
        return std::nullopt;
    }
    return child;
}

}

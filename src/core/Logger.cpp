#include "core/Logger.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <system_error>

namespace citron::log {

namespace {

constexpr std::uintmax_t kRotateAt = 2 * 1024 * 1024;
constexpr int kKeepFiles = 5;

std::mutex g_mutex;
FILE* g_file = nullptr;
Level g_minimum = Level::Debug;
std::filesystem::path g_path;
const auto g_start = std::chrono::steady_clock::now();

const char* levelTag(Level level) {
    switch (level) {
    case Level::Debug: return "debug";
    case Level::Info: return "info ";
    case Level::Warn: return "warn ";
    case Level::Error: return "error";
    }
    return "?    ";
}

void rotate(const std::filesystem::path& directory) {
    std::error_code ec;
    const auto current = directory / "citron.log";
    if (!std::filesystem::exists(current, ec) || std::filesystem::file_size(current, ec) < kRotateAt) {
        return;
    }
    for (int i = kKeepFiles - 1; i >= 1; --i) {
        const auto from = directory / std::format("citron.{}.log", i);
        const auto to = directory / std::format("citron.{}.log", i + 1);
        if (std::filesystem::exists(from, ec)) {
            std::filesystem::rename(from, to, ec);
        }
    }
    std::filesystem::rename(current, directory / "citron.1.log", ec);
}

std::string timestamp() {
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03}", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

}

void open(const std::filesystem::path& directory) {
    std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    rotate(directory);
    g_path = directory / "citron.log";
    g_file = _wfsopen(g_path.c_str(), L"ab", _SH_DENYNO);
}

void close() {
    std::lock_guard lock(g_mutex);
    if (g_file != nullptr) {
        std::fclose(g_file);
        g_file = nullptr;
    }
}

void setMinimumLevel(Level level) {
    std::lock_guard lock(g_mutex);
    g_minimum = level;
}

void write(Level level, std::string_view message) {
    std::lock_guard lock(g_mutex);
    if (level < g_minimum) {
        return;
    }
    const std::string line = std::format("{} [{}] [{:5}] {}\n", timestamp(), levelTag(level), GetCurrentThreadId(), message);
    if (g_file != nullptr) {
        std::fwrite(line.data(), 1, line.size(), g_file);
        std::fflush(g_file);
    }
    if (IsDebuggerPresent()) {
        OutputDebugStringA(line.c_str());
    }
}

double elapsedMs() {
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(now - g_start).count();
}

std::filesystem::path currentFile() {
    std::lock_guard lock(g_mutex);
    return g_path;
}

}

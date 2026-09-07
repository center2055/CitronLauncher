#pragma once

#include <filesystem>
#include <format>
#include <string_view>

namespace citron::log {

enum class Level {
    Debug,
    Info,
    Warn,
    Error,
};

void open(const std::filesystem::path& directory);
void close();
void setMinimumLevel(Level level);
void write(Level level, std::string_view message);
double elapsedMs();
std::filesystem::path currentFile();

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Debug, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    write(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

}

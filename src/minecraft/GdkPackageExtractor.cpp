#include "minecraft/GdkPackageExtractor.h"

#include "core/Json.h"
#include "core/Text.h"
#include "platform/windows/FileOps.h"
#include "resource.h"

#include <windows.h>

#include <atomic>
#include <format>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace citron {

namespace {

constexpr wchar_t kCoreDll[] = L"launcher_core.dll";
constexpr wchar_t kApiDll[] = L"launcher_api.dll";
constexpr wchar_t kHttpDll[] = L"libHttpClient.dll";
constexpr wchar_t kRuntimeDll[] = L"vcruntime140_1.dll";
constexpr wchar_t kRuntimeBaseDll[] = L"vcruntime140.dll";
constexpr wchar_t kCppRuntimeDll[] = L"msvcp140.dll";
constexpr wchar_t kCodecvtRuntimeDll[] = L"msvcp140_codecvt_ids.dll";

using ExtractWithPipe = int(WINAPI*)(const char* package, const char* outputDirectory, const char* pipeName);
using ExtractWide = int(WINAPI*)(const wchar_t* package, const wchar_t* outputDirectory);
using ExtractAnsi = int(WINAPI*)(const char* package, const char* outputDirectory);

// The upstream DLL exposes process-wide environment variables and relies on a
// directory-wide dependency search path. Serialising calls avoids races with
// another extraction or any temporary environment restoration.
std::mutex g_extractorMutex;

Error extractorError(std::string message, std::string detail = {}, bool retryable = false) {
    return Error::make(ErrorCategory::Gdk, "extract GDK package", std::move(message), std::move(detail), retryable);
}

std::string nativeResultMessage(int result) {
    switch (result) {
    case 0: return {};
    case 1: return "The native extractor reported an internal exception.";
    case 2: return "The native extractor rejected its parameters.";
    case 3: return "The native extractor could not find a required key.";
    case 4: return "The native extractor rejected this caller.";
    case 5: return "The native extractor could not open its progress pipe.";
    case 6: return "The package file was not found.";
    case 7: return "The extraction directory is invalid.";
    case 8: return "The package could not be parsed.";
    case 9: return "The native extractor could not extract this package.";
    default: return std::format("The native extractor failed with code {}.", result);
    }
}

std::optional<std::string> toAnsiPath(const std::filesystem::path& path) {
    const std::wstring value = path.wstring();
    BOOL usedDefault = FALSE;
    const int length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, &usedDefault);
    if (length <= 0 || usedDefault) {
        return std::nullopt;
    }
    std::string result(static_cast<size_t>(length), '\0');
    if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, value.c_str(), static_cast<int>(value.size()), result.data(), length, nullptr, &usedDefault) <= 0 || usedDefault) {
        return std::nullopt;
    }
    return result;
}

class EnvironmentValue {
public:
    explicit EnvironmentValue(const wchar_t* name) : name_(name) {
        const DWORD needed = GetEnvironmentVariableW(name_, nullptr, 0);
        if (needed != 0) {
            std::wstring value(needed, L'\0');
            if (GetEnvironmentVariableW(name_, value.data(), needed) != 0) {
                value.resize(value.size() - 1);
                previous_ = std::move(value);
            }
        }
    }

    ~EnvironmentValue() {
        SetEnvironmentVariableW(name_, previous_ ? previous_->c_str() : nullptr);
    }

    bool set(const std::filesystem::path& value) const {
        return SetEnvironmentVariableW(name_, value.c_str()) != FALSE;
    }

private:
    const wchar_t* name_;
    std::optional<std::wstring> previous_;
};

class ProgressPipe {
public:
    explicit ProgressPipe(ExtractionProgressSink progress) : progress_(std::move(progress)) {
        const auto ticks = static_cast<unsigned long long>(GetTickCount64());
        name_ = std::format(L"\\\\.\\pipe\\citron_gdk_{:X}_{:X}", GetCurrentProcessId(), ticks);
        pipe_ = CreateNamedPipeW(name_.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                 1, 4096, 4096, 0, nullptr);
    }

    ~ProgressPipe() {
        stop();
    }

    bool valid() const { return pipe_ != INVALID_HANDLE_VALUE; }
    const std::wstring& name() const { return name_; }

    void start() {
        reader_ = std::jthread([this] { read(); });
    }

    void stop() {
        stopRequested_.store(true);
        if (reader_.joinable()) {
            CancelSynchronousIo(reader_.native_handle());
            reader_.join();
        }
        if (pipe_ != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(pipe_);
            CloseHandle(pipe_);
            pipe_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    void reportLine(std::string_view line) {
        if (!progress_) {
            return;
        }
        const auto parsed = json::parse(line);
        if (!parsed || !parsed->isObject()) {
            return;
        }
        std::uint64_t current = (*parsed)["global_current"].asUnsigned();
        std::uint64_t total = (*parsed)["global_total"].asUnsigned();
        if (total == 0) {
            current = (*parsed)["current"].asUnsigned();
            total = (*parsed)["total"].asUnsigned();
        }
        if (total != 0) {
            progress_(current, total);
        }
    }

    void read() {
        const BOOL connected = ConnectNamedPipe(pipe_, nullptr);
        if (!connected && GetLastError() != ERROR_PIPE_CONNECTED) {
            return;
        }
        std::string pending;
        char buffer[4096];
        while (!stopRequested_.load()) {
            DWORD received = 0;
            if (!ReadFile(pipe_, buffer, sizeof(buffer), &received, nullptr) || received == 0) {
                return;
            }
            pending.append(buffer, received);
            size_t newline = 0;
            while ((newline = pending.find('\n')) != std::string::npos) {
                std::string line = pending.substr(0, newline);
                pending.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                reportLine(line);
            }
        }
    }

    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::wstring name_;
    ExtractionProgressSink progress_;
    std::atomic_bool stopRequested_ = false;
    std::jthread reader_;
};

class LoadedModule {
public:
    explicit LoadedModule(HMODULE module = nullptr) : module_(module) {}
    ~LoadedModule() { if (module_ != nullptr) FreeLibrary(module_); }
    LoadedModule(const LoadedModule&) = delete;
    LoadedModule& operator=(const LoadedModule&) = delete;
    LoadedModule(LoadedModule&& other) noexcept : module_(std::exchange(other.module_, nullptr)) {}
    LoadedModule& operator=(LoadedModule&& other) noexcept {
        if (this != &other) {
            if (module_ != nullptr) FreeLibrary(module_);
            module_ = std::exchange(other.module_, nullptr);
        }
        return *this;
    }
    HMODULE get() const { return module_; }

private:
    HMODULE module_;
};

Result<void> writeEmbeddedFile(unsigned resourceId, const std::filesystem::path& destination) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (resource == nullptr) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Gdk, "find embedded native component", GetLastError(),
                                                 "Citron's embedded native extractor component is missing."));
    }
    const DWORD size = SizeofResource(nullptr, resource);
    HGLOBAL loaded = LoadResource(nullptr, resource);
    const void* data = loaded == nullptr ? nullptr : LockResource(loaded);
    if (data == nullptr || size == 0) {
        return std::unexpected(extractorError("Citron's embedded native extractor component is invalid."));
    }
    if (platform::fileSize(destination) == size) {
        return {};
    }
    std::error_code ec;
    std::filesystem::create_directories(destination.parent_path(), ec);
    if (ec) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "create native component directory", static_cast<unsigned long>(ec.value()),
                                                 "Citron could not prepare its native component directory."));
    }
    const auto temporary = destination.wstring() + L".partial";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "write native component", GetLastError(),
                                                 "Citron could not provision its native extractor."));
    }
    DWORD written = 0;
    const bool complete = WriteFile(file, data, size, &written, nullptr) && written == size && FlushFileBuffers(file);
    DWORD error = complete ? ERROR_SUCCESS : GetLastError();
    CloseHandle(file);
    if (!complete || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        if (complete) error = GetLastError();
        DeleteFileW(temporary.c_str());
        return std::unexpected(Error::fromWin32(ErrorCategory::Filesystem, "provision native component", error,
                                                 "Citron could not finish provisioning its native extractor."));
    }
    return {};
}

Result<void> provisionNativeDependencies(const std::filesystem::path& directory) {
    struct EmbeddedFile { unsigned resource; const wchar_t* name; };
    const std::vector<EmbeddedFile> files = {
        {IDR_LAUNCHER_CORE, kCoreDll}, {IDR_LAUNCHER_API, kApiDll},
        {IDR_LIB_HTTP_CLIENT, kHttpDll}, {IDR_VCRUNTIME140_1, kRuntimeDll},
        {IDR_VCRUNTIME140, kRuntimeBaseDll}, {IDR_MSVCP140, kCppRuntimeDll},
        {IDR_MSVCP140_CODECVT_IDS, kCodecvtRuntimeDll},
    };
    for (const auto& file : files) {
        if (auto written = writeEmbeddedFile(file.resource, directory / file.name); !written) {
            return std::unexpected(written.error());
        }
    }
    return {};
}

}

Result<void> LauncherCoreGdkExtractor::extract(const std::filesystem::path& package,
                                               const std::filesystem::path& outputDirectory,
                                               const std::filesystem::path& directory,
                                               ExtractionProgressSink progress,
                                               std::stop_token cancellation) {
    if (cancellation.stop_requested()) {
        return std::unexpected(Error::cancelled("extract GDK package"));
    }
    if (!platform::fileExists(package)) {
        return std::unexpected(extractorError("The verified package file is missing.", package.string(), true));
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(outputDirectory, ec)) {
        return std::unexpected(extractorError("The extraction directory is unavailable.", outputDirectory.string(), true));
    }

    std::lock_guard lock(g_extractorMutex);
    if (auto provisioned = provisionNativeDependencies(directory); !provisioned) {
        return std::unexpected(provisioned.error());
    }

    EnvironmentValue coreVariable(L"LAUNCHER_CORE_DLL");
    EnvironmentValue apiVariable(L"LAUNCHER_API_DLL");
    if (!coreVariable.set(directory / kCoreDll) || !apiVariable.set(directory / kApiDll)) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Gdk, "configure native extractor", GetLastError(),
                                                 "The native extractor environment could not be configured."));
    }

    if (!SetDllDirectoryW(directory.c_str())) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Gdk, "configure native extractor", GetLastError(),
                                                 "The native extractor dependency path could not be configured."));
    }
    struct DllDirectoryReset { ~DllDirectoryReset() { SetDllDirectoryW(nullptr); } } reset;

    std::vector<LoadedModule> dependencies;
    for (const auto name : {kRuntimeDll, kHttpDll, kApiDll}) {
        HMODULE loaded = LoadLibraryW((directory / name).c_str());
        if (loaded == nullptr) {
            Error error = Error::fromWin32(ErrorCategory::Gdk, "load native dependency", GetLastError(),
                                           "A native GDK extractor dependency could not be loaded: " + text::toUtf8(name));
            error.retryable = true;
            return std::unexpected(std::move(error));
        }
        dependencies.emplace_back(loaded);
    }

    LoadedModule core(LoadLibraryW((directory / kCoreDll).c_str()));
    if (core.get() == nullptr) {
        Error error = Error::fromWin32(ErrorCategory::Gdk, "load native extractor", GetLastError(), "launcher_core.dll could not be loaded.");
        error.retryable = true;
        return std::unexpected(std::move(error));
    }

    const auto packageFull = std::filesystem::absolute(package, ec);
    const auto outputFull = std::filesystem::absolute(outputDirectory, ec);
    if (ec) {
        return std::unexpected(extractorError("The extraction paths could not be resolved.", ec.message(), true));
    }

    int result = -1;
    if (const auto withPipe = reinterpret_cast<ExtractWithPipe>(GetProcAddress(core.get(), "GetWithPipe"))) {
        const auto packageAnsi = toAnsiPath(packageFull);
        const auto outputAnsi = toAnsiPath(outputFull);
        if (!packageAnsi || !outputAnsi) {
            return std::unexpected(extractorError("This package path contains characters unsupported by launcher_core.dll.",
                "Move Citron and its data folder to a path supported by the current Windows ANSI code page.", true));
        }
        ProgressPipe pipe(std::move(progress));
        if (!pipe.valid()) {
            return std::unexpected(Error::fromWin32(ErrorCategory::Gdk, "create extraction progress pipe", GetLastError(),
                "Citron could not create the native extraction progress channel."));
        }
        pipe.start();
        const auto pipeAnsi = toAnsiPath(pipe.name());
        if (!pipeAnsi) {
            return std::unexpected(extractorError("The native extraction progress pipe could not be encoded."));
        }
        result = withPipe(packageAnsi->c_str(), outputAnsi->c_str(), pipeAnsi->c_str());
        pipe.stop();
    } else if (const auto wide = reinterpret_cast<ExtractWide>(GetProcAddress(core.get(), "GetW"))) {
        result = wide(packageFull.c_str(), outputFull.c_str());
    } else if (const auto ansi = reinterpret_cast<ExtractAnsi>(GetProcAddress(core.get(), "Get"))) {
        const auto packageAnsi = toAnsiPath(packageFull);
        const auto outputAnsi = toAnsiPath(outputFull);
        if (!packageAnsi || !outputAnsi) {
            return std::unexpected(extractorError("This package path contains characters unsupported by launcher_core.dll.",
                "Move Citron and its data folder to a path supported by the current Windows ANSI code page.", true));
        }
        result = ansi(packageAnsi->c_str(), outputAnsi->c_str());
    } else {
        return std::unexpected(extractorError("launcher_core.dll does not expose a supported extraction function.",
            "Expected GetWithPipe, GetW, or Get."));
    }

    if (cancellation.stop_requested()) {
        return std::unexpected(Error::cancelled("extract GDK package"));
    }
    if (result != 0) {
        return std::unexpected(extractorError(nativeResultMessage(result), std::format("native result {}", result), true));
    }
    return {};
}

}

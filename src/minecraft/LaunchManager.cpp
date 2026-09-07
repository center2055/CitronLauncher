#include "minecraft/LaunchManager.h"

#include "core/Logger.h"
#include "core/Text.h"
#include "platform/windows/Activation.h"
#include "platform/windows/PackageManager.h"
#include "platform/windows/Process.h"

#include <windows.h>

#include <chrono>
#include <format>
#include <thread>

namespace citron {

namespace {

constexpr auto kGameExe = L"Minecraft.Windows.exe";
constexpr auto kHelperTimeout = std::chrono::seconds(180);
constexpr auto kEarlyExitWindow = std::chrono::seconds(8);

Result<void> launchDirect(VersionChannel channel, const std::filesystem::path& installLocation, std::stop_token token) {
    const std::wstring family = text::toWide(packageFamilyName(channel));
    std::wstring aumid = platform::appUserModelId(family).value_or(family + L"!Game");
    const std::wstring exe = platform::gameExecutable(installLocation);
    log::info("activating {} in {}", text::toUtf8(exe), text::toUtf8(aumid));
    auto started = platform::activatePackageExecutable(aumid, exe, installLocation);
    if (!started) {
        return std::unexpected(started.error());
    }
    HANDLE process = static_cast<HANDLE>(started->handle);
    if (process == nullptr) {
        return {};
    }
    const auto deadline = std::chrono::steady_clock::now() + kEarlyExitWindow;
    Result<void> result;
    while (std::chrono::steady_clock::now() < deadline) {
        if (token.stop_requested()) {
            break;
        }
        const DWORD wait = WaitForSingleObject(process, 250);
        if (wait == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(process, &code);
            result = std::unexpected(Error::make(ErrorCategory::Launch, "launch", "Minecraft closed right after starting.", std::format("exit code 0x{:08X}", code)));
            break;
        }
    }
    CloseHandle(process);
    if (result) {
        log::info("minecraft running as pid {}", started->pid);
    }
    return result;
}

Result<void> launchThroughHelper(VersionChannel channel, const std::filesystem::path& installLocation, std::stop_token token) {
    auto started = platform::launchPackage(text::toWide(packageFamilyName(channel)));
    if (!started) {
        return std::unexpected(started.error());
    }
    const auto deadline = std::chrono::steady_clock::now() + kHelperTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (token.stop_requested()) {
            return std::unexpected(Error::cancelled("launch"));
        }
        if (platform::anyProcessUnder(kGameExe, installLocation)) {
            log::info("minecraft process detected");
            return {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    return std::unexpected(Error::make(ErrorCategory::Launch, "launch", "Minecraft did not start.", "no game process appeared within three minutes"));
}

}

LaunchManager::LaunchManager(TaskScheduler& scheduler) : scheduler_(scheduler) {}

bool LaunchManager::busy() const {
    return operation_ && !operation_->done();
}

bool LaunchManager::launch(VersionChannel channel, std::filesystem::path installLocation, LaunchMode mode, LaunchDone done) {
    if (busy()) {
        return false;
    }
    auto job = [channel, installLocation = std::move(installLocation), mode, done = std::move(done)](std::stop_token token) {
        platform::initializeApartment();
        if (platform::anyProcessUnder(kGameExe, installLocation)) {
            done(std::unexpected(Error::make(ErrorCategory::Launch, "launch", "Minecraft is already running.")));
            return;
        }
        log::info("launching {} from {} ({})", channelName(channel), installLocation.string(), mode == LaunchMode::Direct ? "direct" : "helper");
        Result<void> result;
        if (mode == LaunchMode::Direct) {
            result = launchDirect(channel, installLocation, token);
            if (!result && !result.error().isCancelled() && result.error().detail.find("exit code") == std::string::npos) {
                log::warn("direct launch failed, falling back to the launch helper: {}", result.error().summary());
                result = launchThroughHelper(channel, installLocation, token);
            }
        } else {
            result = launchThroughHelper(channel, installLocation, token);
        }
        done(std::move(result));
    };
    operation_ = scheduler_.start("launch", std::move(job));
    return true;
}

}

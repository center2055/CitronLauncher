#include "platform/windows/PackageManager.h"

#include "core/Logger.h"
#include "core/Text.h"

#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Storage.h>

#include <chrono>
#include <thread>

namespace citron::platform {

namespace {

namespace wf = winrt::Windows::Foundation;
namespace wam = winrt::Windows::ApplicationModel;
namespace wmd = winrt::Windows::Management::Deployment;

InstalledPackage describe(const wam::Package& package) {
    InstalledPackage info;
    const auto id = package.Id();
    info.fullName = std::wstring(id.FullName());
    info.familyName = std::wstring(id.FamilyName());
    info.name = std::wstring(id.Name());
    const auto version = id.Version();
    info.version = std::format(L"{}.{}.{}.{}", version.Major, version.Minor, version.Build, version.Revision);
    try {
        info.installLocation = std::wstring(package.InstalledPath());
    } catch (const winrt::hresult_error&) {
    }
    try {
        info.developmentMode = package.IsDevelopmentMode();
    } catch (const winrt::hresult_error&) {
    }
    try {
        info.isFramework = package.IsFramework();
    } catch (const winrt::hresult_error&) {
    }
    return info;
}

Error deploymentError(std::string operation, const winrt::hresult_error& e, std::string message) {
    Error error = Error::fromHresult(ErrorCategory::Package, std::move(operation), e.code().value, std::move(message));
    const std::string text = text::toUtf8(std::wstring_view(e.message()));
    if (!text.empty()) {
        error.detail = text;
    }
    return error;
}

template <typename Op>
bool waitForOperation(Op& op, std::stop_token token) {
    while (op.Status() == wf::AsyncStatus::Started) {
        if (token.stop_requested()) {
            op.Cancel();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    while (op.Status() == wf::AsyncStatus::Started) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return op.Status() == wf::AsyncStatus::Completed;
}

Result<void> checkResult(const wmd::DeploymentResult& result, std::string operation, std::string message, std::stop_token token) {
    const auto code = result.ExtendedErrorCode();
    if (code.value == 0) {
        return {};
    }
    if (token.stop_requested()) {
        return std::unexpected(Error::cancelled(operation));
    }
    Error error = Error::fromHresult(ErrorCategory::Package, std::move(operation), code.value, std::move(message));
    const std::string text = text::trim(text::toUtf8(std::wstring_view(result.ErrorText())));
    if (!text.empty()) {
        error.detail = text;
    }
    error.retryable = true;
    return std::unexpected(error);
}

}

void initializeApartment() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
}

std::vector<InstalledPackage> findPackagesByFamily(std::wstring_view familyName) {
    std::vector<InstalledPackage> out;
    try {
        wmd::PackageManager manager;
        for (const auto& package : manager.FindPackagesForUser(L"", winrt::hstring(familyName))) {
            out.push_back(describe(package));
        }
    } catch (const winrt::hresult_error& e) {
        log::warn("find packages {} failed: 0x{:08X}", text::toUtf8(familyName), static_cast<unsigned long>(e.code().value));
    }
    return out;
}

std::vector<InstalledPackage> findPackagesByName(std::wstring_view name) {
    std::vector<InstalledPackage> out;
    try {
        wmd::PackageManager manager;
        for (const auto& package : manager.FindPackagesForUser(L"")) {
            const auto id = package.Id();
            if (text::equalsIgnoreCase(text::toUtf8(std::wstring_view(id.Name())), text::toUtf8(name))) {
                out.push_back(describe(package));
            }
        }
    } catch (const winrt::hresult_error& e) {
        log::warn("find packages by name {} failed: 0x{:08X}", text::toUtf8(name), static_cast<unsigned long>(e.code().value));
    }
    return out;
}

std::optional<InstalledPackage> findPackageByName(std::wstring_view name) {
    const auto packages = findPackagesByName(name);
    return packages.empty() ? std::nullopt : std::optional<InstalledPackage>(packages.front());
}

Result<void> removePackage(std::wstring_view fullName, std::stop_token token) {
    const std::string operation = "remove package";
    try {
        wmd::PackageManager manager;
        auto op = manager.RemovePackageAsync(winrt::hstring(fullName), wmd::RemovalOptions::PreserveRoamableApplicationData);
        if (!waitForOperation(op, token)) {
            if (token.stop_requested()) {
                return std::unexpected(Error::cancelled(operation));
            }
            return std::unexpected(Error::fromHresult(ErrorCategory::Package, operation, op.ErrorCode().value, "Minecraft could not be removed."));
        }
        return checkResult(op.GetResults(), operation, "Minecraft could not be removed.", token);
    } catch (const winrt::hresult_error& e) {
        return std::unexpected(deploymentError(operation, e, "Minecraft could not be removed."));
    }
}

Result<void> launchPackage(std::wstring_view familyName) {
    const std::string operation = "launch";
    try {
        wmd::PackageManager manager;
        for (const auto& package : manager.FindPackagesForUser(L"", winrt::hstring(familyName))) {
            auto entries = package.GetAppListEntriesAsync().get();
            for (const auto& entry : entries) {
                if (entry.LaunchAsync().get()) {
                    return {};
                }
            }
            return std::unexpected(Error::make(ErrorCategory::Launch, operation, "Minecraft could not be started.", "The package has no launchable application entry."));
        }
        return std::unexpected(Error::make(ErrorCategory::Launch, operation, "Minecraft is not installed.", text::toUtf8(familyName)));
    } catch (const winrt::hresult_error& e) {
        return std::unexpected(deploymentError(operation, e, "Minecraft could not be started."));
    }
}

}

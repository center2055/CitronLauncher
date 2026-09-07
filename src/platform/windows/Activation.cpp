#include "platform/windows/Activation.h"

#include "core/Logger.h"
#include "core/Settings.h"
#include "core/Text.h"

#include <windows.h>
#include <objbase.h>
#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>

#include <regex>

namespace citron::platform {

namespace {

// desktop package activator hosted by the shell, the same object the
// appx powershell module uses to run a command inside a package
constexpr GUID kDesktopAppxActivator = {0x168EB462, 0x775F, 0x42AE, {0x91, 0x11, 0xD7, 0x14, 0xB2, 0x30, 0x6C, 0x2E}};

struct __declspec(uuid("F158268A-D5A5-45CE-99CF-00D6C3F3FC0A")) IDesktopAppXActivator : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Activate(LPCWSTR appUserModelId, LPCWSTR packageRelativeExecutable, LPCWSTR arguments, HANDLE* process) = 0;
    virtual HRESULT STDMETHODCALLTYPE ActivateWithOptions(LPCWSTR appUserModelId, LPCWSTR executable, LPCWSTR arguments, ULONG options, DWORD parentProcessId, HANDLE* process) = 0;
    virtual HRESULT STDMETHODCALLTYPE ActivateWithOptionsAndArgs(LPCWSTR appUserModelId, LPCWSTR executable, LPCWSTR arguments, ULONG options, DWORD parentProcessId, IUnknown* activatedEventArgs, HANDLE* process) = 0;
    virtual HRESULT STDMETHODCALLTYPE ActivateWithOptionsArgsWorkingDirectoryShowWindow(LPCWSTR appUserModelId, LPCWSTR executable, LPCWSTR arguments, ULONG options, DWORD parentProcessId, IUnknown* activatedEventArgs, LPCWSTR workingDirectory, ULONG showWindow, HANDLE* process) = 0;
};

constexpr wchar_t kDefaultExecutable[] = L"Minecraft.Windows.exe";

}

Result<ActivatedProcess> activatePackageExecutable(std::wstring_view appUserModelId, std::wstring_view executable, const std::filesystem::path& workingDirectory) {
    const std::string operation = "activate";
    winrt::com_ptr<IDesktopAppXActivator> activator;
    HRESULT hr = CoCreateInstance(kDesktopAppxActivator, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IDesktopAppXActivator), activator.put_void());
    if (FAILED(hr)) {
        return std::unexpected(Error::fromHresult(ErrorCategory::Launch, operation, hr, "Minecraft could not be started with its package identity."));
    }
    const std::wstring aumid(appUserModelId);
    const std::wstring exe(executable);
    const std::wstring directory = workingDirectory.wstring();
    HANDLE process = nullptr;
    hr = activator->ActivateWithOptionsArgsWorkingDirectoryShowWindow(aumid.c_str(), exe.c_str(), L"", 0, GetCurrentProcessId(), nullptr,
                                                                      directory.empty() ? nullptr : directory.c_str(), SW_SHOWNORMAL, &process);
    if (FAILED(hr)) {
        hr = activator->ActivateWithOptions(aumid.c_str(), exe.c_str(), L"", 0, GetCurrentProcessId(), &process);
    }
    if (FAILED(hr)) {
        return std::unexpected(Error::fromHresult(ErrorCategory::Launch, operation, hr, "Minecraft could not be started."));
    }
    ActivatedProcess result;
    result.handle = process;
    result.pid = process != nullptr ? GetProcessId(process) : 0;
    return result;
}

std::optional<std::wstring> appUserModelId(std::wstring_view familyName) {
    try {
        winrt::Windows::Management::Deployment::PackageManager manager;
        for (const auto& package : manager.FindPackagesForUser(L"", winrt::hstring(familyName))) {
            auto entries = package.GetAppListEntriesAsync().get();
            for (const auto& entry : entries) {
                return std::wstring(entry.AppUserModelId());
            }
        }
    } catch (const winrt::hresult_error& e) {
        log::warn("app list lookup failed: 0x{:08X}", static_cast<unsigned long>(e.code().value));
    }
    return std::nullopt;
}

std::wstring gameExecutable(const std::filesystem::path& installLocation) {
    auto content = readFile(installLocation / L"MicrosoftGame.Config");
    if (!content) {
        return kDefaultExecutable;
    }
    const std::regex executable(R"(<Executable\b[^>]*>)", std::regex::icase);
    const std::regex name(R"re(Name\s*=\s*"([^"]+)")re", std::regex::icase);
    const std::regex family(R"re(TargetDeviceFamily\s*=\s*"([^"]+)")re", std::regex::icase);
    std::string fallback;
    for (auto it = std::sregex_iterator(content->begin(), content->end(), executable); it != std::sregex_iterator(); ++it) {
        const std::string element = it->str();
        std::smatch match;
        if (!std::regex_search(element, match, name)) {
            continue;
        }
        const std::string value = match[1].str();
        if (value.find('\\') != std::string::npos || value.find('/') != std::string::npos || value.find("..") != std::string::npos) {
            continue;
        }
        std::smatch familyMatch;
        if (std::regex_search(element, familyMatch, family) && text::equalsIgnoreCase(familyMatch[1].str(), "PC")) {
            return text::toWide(value);
        }
        if (fallback.empty()) {
            fallback = value;
        }
    }
    return fallback.empty() ? std::wstring(kDefaultExecutable) : text::toWide(fallback);
}

}

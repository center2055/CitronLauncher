#include "platform/windows/Gdk.h"

#include "platform/windows/PackageManager.h"
#include "platform/windows/Registry.h"

#include <windows.h>

#include <format>

namespace citron::platform {

namespace {

std::wstring windowsVersionText() {
    const auto build = registryReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuildNumber");
    const auto display = registryReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
    const auto product = registryReadString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
    std::wstring out = product.value_or(L"Windows");
    if (display) {
        out += L" " + *display;
    }
    if (build) {
        out += L" (build " + *build + L")";
    }
    return out;
}

}

GdkEnvironment inspectEnvironment() {
    GdkEnvironment env;
    env.windowsVersion = windowsVersionText();
    if (auto services = findPackageByName(L"Microsoft.GamingServices")) {
        env.gamingServices = true;
        env.gamingServicesVersion = services->version;
    }
    env.gameInput = registryKeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\GameInput") ||
                    registryKeyExists(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\GameInputRedist");
    env.vcLibs = !findPackagesByFamily(L"Microsoft.VCLibs.140.00.UWPDesktop_8wekyb3d8bbwe").empty();
    env.appRuntime = !findPackagesByFamily(L"Microsoft.WindowsAppRuntime.1.8_8wekyb3d8bbwe").empty();
    env.xboxIdentity = !findPackagesByFamily(L"Microsoft.XboxIdentityProvider_8wekyb3d8bbwe").empty();
    return env;
}

const wchar_t* gamingServicesStoreUrl() {
    return L"ms-windows-store://pdp/?productid=9MWPM2CQNLHN";
}

const wchar_t* xboxAppStoreUrl() {
    return L"ms-windows-store://pdp/?productid=9MV0B5HZVK9Z";
}

}

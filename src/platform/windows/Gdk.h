#pragma once

#include <string>

namespace citron::platform {

struct GdkEnvironment {
    bool gamingServices = false;
    std::wstring gamingServicesVersion;
    bool gameInput = false;
    bool vcLibs = false;
    bool appRuntime = false;
    bool xboxIdentity = false;
    std::wstring windowsVersion;
};

GdkEnvironment inspectEnvironment();
const wchar_t* gamingServicesStoreUrl();
const wchar_t* xboxAppStoreUrl();

}

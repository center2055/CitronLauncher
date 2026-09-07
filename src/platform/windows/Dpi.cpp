#include "platform/windows/Dpi.h"

#include <cmath>

namespace citron::platform {

UINT dpiForWindow(HWND hwnd) {
    const UINT dpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : GetDpiForSystem();
    return dpi == 0 ? kBaseDpi : dpi;
}

float scaleForDpi(UINT dpi) {
    return static_cast<float>(dpi) / static_cast<float>(kBaseDpi);
}

int dipsToPixels(float dips, UINT dpi) {
    return static_cast<int>(std::lround(dips * scaleForDpi(dpi)));
}

float pixelsToDips(int pixels, UINT dpi) {
    return static_cast<float>(pixels) / scaleForDpi(dpi);
}

int systemMetricForDpi(int index, UINT dpi) {
    return GetSystemMetricsForDpi(index, dpi);
}

}

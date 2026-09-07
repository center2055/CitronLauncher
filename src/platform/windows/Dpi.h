#pragma once

#include <windows.h>

namespace citron::platform {

constexpr UINT kBaseDpi = 96;

UINT dpiForWindow(HWND hwnd);
float scaleForDpi(UINT dpi);
int dipsToPixels(float dips, UINT dpi);
float pixelsToDips(int pixels, UINT dpi);
int systemMetricForDpi(int index, UINT dpi);

}

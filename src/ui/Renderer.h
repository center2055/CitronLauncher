#pragma once

#include "core/Error.h"
#include "ui/Geometry.h"

#include <windows.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite_3.h>
#include <dxgi1_3.h>
#include <winrt/base.h>

#include <map>
#include <span>
#include <string_view>
#include <tuple>

namespace citron::ui {

enum class Font {
    Body,
    Title,
    Mono,
};

struct TextStyle {
    Font font = Font::Body;
    float size = 14.0f;
    int weight = 400;
    float letterSpacing = 0.0f;
};

enum class Align {
    Start,
    Center,
    End,
};

struct TextOptions {
    Align horizontal = Align::Start;
    Align vertical = Align::Center;
    bool ellipsis = true;
    bool wrap = false;
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Result<void> initialize(HWND hwnd, UINT dpi, std::span<const std::uint8_t> bodyFont, std::span<const std::uint8_t> titleFont, std::span<const std::uint8_t> monoFont);
    void resize(UINT widthPixels, UINT heightPixels);
    void setDpi(UINT dpi);
    bool beginFrame(Color clear);
    void endFrame();
    bool lost() const { return lost_; }
    Result<void> recover();

    void fillRect(const Rect& rect, Color color, float radius = 0.0f);
    void strokeRect(const Rect& rect, Color color, float width = 1.0f, float radius = 0.0f);
    void fillCircle(Point center, float radius, Color color);
    void drawLine(Point a, Point b, Color color, float width = 1.0f);
    void drawText(std::wstring_view text, const TextStyle& style, const Rect& bounds, Color color, const TextOptions& options = {});
    Size measureText(std::wstring_view text, const TextStyle& style, float maxWidth = 1e6f, bool wrap = false);
    winrt::com_ptr<IDWriteTextLayout> createLayout(std::wstring_view text, const TextStyle& style, float maxWidth, float maxHeight, bool wrap);
    void drawLayout(IDWriteTextLayout* layout, Point origin, Color color);
    void pushClip(const Rect& rect);
    void popClip();
    void pushOpacity(const Rect& rect, float opacity);
    void popOpacity();
    void fillGeometry(ID2D1Geometry* geometry, Color color, const D2D1_MATRIX_3X2_F& transform);
    void strokeGeometry(ID2D1Geometry* geometry, Color color, float width, const D2D1_MATRIX_3X2_F& transform, bool roundCaps = true);
    void drawBitmap(ID2D1Bitmap* bitmap, const Rect& dest, float opacity = 1.0f, float radius = 0.0f);
    winrt::com_ptr<ID2D1Bitmap1> loadPng(std::span<const std::uint8_t> bytes);

    ID2D1Factory2* factory() const { return factory_.get(); }
    ID2D1DeviceContext1* context() const { return context_.get(); }
    IDWriteFactory6* writeFactory() const { return writeFactory_.get(); }
    unsigned generation() const { return generation_; }
    float dpiScale() const { return static_cast<float>(dpi_) / 96.0f; }

private:
    Result<void> createDevice();
    Result<void> createSwapChain();
    Result<void> createTarget();
    Result<void> createFonts();
    void releaseTarget();
    void releaseDevice();
    IDWriteTextFormat3* format(const TextStyle& style);
    ID2D1SolidColorBrush* brush(Color color);

    HWND hwnd_ = nullptr;
    UINT dpi_ = 96;
    UINT width_ = 1;
    UINT height_ = 1;
    bool lost_ = false;
    bool drawing_ = false;
    unsigned generation_ = 0;
    std::span<const std::uint8_t> bodyFontData_;
    std::span<const std::uint8_t> titleFontData_;
    std::span<const std::uint8_t> monoFontData_;

    winrt::com_ptr<ID3D11Device> d3d_;
    winrt::com_ptr<IDXGIDevice> dxgi_;
    winrt::com_ptr<IDXGISwapChain1> swapChain_;
    winrt::com_ptr<ID2D1Factory2> factory_;
    winrt::com_ptr<ID2D1Device1> device_;
    winrt::com_ptr<ID2D1DeviceContext1> context_;
    winrt::com_ptr<ID2D1Bitmap1> target_;
    winrt::com_ptr<ID2D1SolidColorBrush> brush_;
    winrt::com_ptr<IDCompositionDevice> compDevice_;
    winrt::com_ptr<IDCompositionTarget> compTarget_;
    winrt::com_ptr<IDCompositionVisual> compVisual_;
    winrt::com_ptr<IDWriteFactory6> writeFactory_;
    winrt::com_ptr<IDWriteFontCollection1> fonts_;
    winrt::com_ptr<IDWriteInMemoryFontFileLoader> fontLoader_;
    winrt::com_ptr<IDWriteFontSet> fontSet_;
    std::map<std::tuple<int, int, int>, winrt::com_ptr<IDWriteTextFormat3>> formats_;
    std::map<std::tuple<int, int, int>, winrt::com_ptr<IDWriteInlineObject>> ellipsis_;
};

}

#include "ui/Renderer.h"

#include "core/Logger.h"

#include <wincodec.h>

#include <cmath>

namespace citron::ui {

namespace {

D2D1_COLOR_F toD2D(Color c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

D2D1_RECT_F toD2D(const Rect& r) {
    return D2D1::RectF(r.x, r.y, r.right(), r.bottom());
}

Error graphicsError(const char* operation, HRESULT hr) {
    return Error::fromHresult(ErrorCategory::Internal, operation, hr, "The graphics device could not be initialized.");
}

constexpr float kStrokeAlignInset = 0.5f;

}

Renderer::Renderer() = default;

Renderer::~Renderer() {
    releaseTarget();
    releaseDevice();
}

Result<void> Renderer::initialize(HWND hwnd, UINT dpi, std::span<const std::uint8_t> bodyFont, std::span<const std::uint8_t> titleFont, std::span<const std::uint8_t> monoFont) {
    hwnd_ = hwnd;
    dpi_ = dpi;
    bodyFontData_ = bodyFont;
    titleFontData_ = titleFont;
    monoFontData_ = monoFont;
    RECT rc{};
    GetClientRect(hwnd, &rc);
    width_ = std::max<UINT>(1, static_cast<UINT>(rc.right - rc.left));
    height_ = std::max<UINT>(1, static_cast<UINT>(rc.bottom - rc.top));

    D2D1_FACTORY_OPTIONS options{};
#ifdef _DEBUG
    options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory2), &options, factory_.put_void());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create d2d factory", hr));
    }
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory6), reinterpret_cast<IUnknown**>(writeFactory_.put()));
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create dwrite factory", hr));
    }
    if (auto fonts = createFonts(); !fonts) {
        return fonts;
    }
    return createDevice();
}

Result<void> Renderer::createFonts() {
    HRESULT hr = writeFactory_->CreateInMemoryFontFileLoader(fontLoader_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create font loader", hr));
    }
    hr = writeFactory_->RegisterFontFileLoader(fontLoader_.get());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("register font loader", hr));
    }
    winrt::com_ptr<IDWriteFontSetBuilder1> builder;
    hr = writeFactory_->CreateFontSetBuilder(builder.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create font set builder", hr));
    }
    for (const auto& data : {bodyFontData_, titleFontData_, monoFontData_}) {
        if (data.empty()) {
            continue;
        }
        winrt::com_ptr<IDWriteFontFile> file;
        hr = fontLoader_->CreateInMemoryFontFileReference(writeFactory_.get(), data.data(), static_cast<UINT32>(data.size()), nullptr, file.put());
        if (FAILED(hr)) {
            return std::unexpected(graphicsError("load embedded font", hr));
        }
        hr = builder->AddFontFile(file.get());
        if (FAILED(hr)) {
            return std::unexpected(graphicsError("add embedded font", hr));
        }
    }
    hr = builder->CreateFontSet(fontSet_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create font set", hr));
    }
    hr = writeFactory_->CreateFontCollectionFromFontSet(fontSet_.get(), DWRITE_FONT_FAMILY_MODEL_TYPOGRAPHIC, reinterpret_cast<IDWriteFontCollection2**>(fonts_.put()));
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create font collection", hr));
    }
    return {};
}

Result<void> Renderer::createDevice() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, d3d_.put(), nullptr, nullptr);
    if (FAILED(hr)) {
        log::warn("hardware d3d device unavailable (0x{:08X}), using warp", static_cast<unsigned long>(hr));
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION, d3d_.put(), nullptr, nullptr);
    }
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create d3d device", hr));
    }
    dxgi_ = d3d_.try_as<IDXGIDevice>();
    if (!dxgi_) {
        return std::unexpected(graphicsError("query dxgi device", E_NOINTERFACE));
    }
    hr = factory_->CreateDevice(dxgi_.get(), device_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create d2d device", hr));
    }
    hr = device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, context_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create d2d context", hr));
    }
    context_->SetDpi(static_cast<float>(dpi_), static_cast<float>(dpi_));
    context_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    hr = context_->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), brush_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create brush", hr));
    }
    if (auto chain = createSwapChain(); !chain) {
        return chain;
    }
    hr = DCompositionCreateDevice(dxgi_.get(), __uuidof(IDCompositionDevice), compDevice_.put_void());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create composition device", hr));
    }
    hr = compDevice_->CreateTargetForHwnd(hwnd_, TRUE, compTarget_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create composition target", hr));
    }
    hr = compDevice_->CreateVisual(compVisual_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create composition visual", hr));
    }
    compVisual_->SetContent(swapChain_.get());
    compTarget_->SetRoot(compVisual_.get());
    hr = compDevice_->Commit();
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("commit composition", hr));
    }
    lost_ = false;
    ++generation_;
    return createTarget();
}

Result<void> Renderer::createSwapChain() {
    winrt::com_ptr<IDXGIAdapter> adapter;
    HRESULT hr = dxgi_->GetAdapter(adapter.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("get adapter", hr));
    }
    winrt::com_ptr<IDXGIFactory2> dxgiFactory;
    hr = adapter->GetParent(__uuidof(IDXGIFactory2), dxgiFactory.put_void());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("get dxgi factory", hr));
    }
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width_;
    desc.Height = height_;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    hr = dxgiFactory->CreateSwapChainForComposition(d3d_.get(), &desc, nullptr, swapChain_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create swap chain", hr));
    }
    return {};
}

Result<void> Renderer::createTarget() {
    winrt::com_ptr<IDXGISurface> surface;
    HRESULT hr = swapChain_->GetBuffer(0, __uuidof(IDXGISurface), surface.put_void());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("get back buffer", hr));
    }
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
                                               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                                               static_cast<float>(dpi_), static_cast<float>(dpi_));
    hr = context_->CreateBitmapFromDxgiSurface(surface.get(), &props, target_.put());
    if (FAILED(hr)) {
        return std::unexpected(graphicsError("create target bitmap", hr));
    }
    context_->SetTarget(target_.get());
    return {};
}

void Renderer::releaseTarget() {
    if (context_) {
        context_->SetTarget(nullptr);
    }
    target_ = nullptr;
}

void Renderer::releaseDevice() {
    releaseTarget();
    compVisual_ = nullptr;
    compTarget_ = nullptr;
    compDevice_ = nullptr;
    brush_ = nullptr;
    context_ = nullptr;
    device_ = nullptr;
    swapChain_ = nullptr;
    dxgi_ = nullptr;
    d3d_ = nullptr;
}

Result<void> Renderer::recover() {
    log::warn("graphics device lost, recreating");
    releaseDevice();
    return createDevice();
}

void Renderer::resize(UINT widthPixels, UINT heightPixels) {
    widthPixels = std::max<UINT>(1, widthPixels);
    heightPixels = std::max<UINT>(1, heightPixels);
    if (widthPixels == width_ && heightPixels == height_) {
        return;
    }
    width_ = widthPixels;
    height_ = heightPixels;
    if (!swapChain_) {
        return;
    }
    releaseTarget();
    const HRESULT hr = swapChain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        lost_ = true;
        return;
    }
    if (auto target = createTarget(); !target) {
        lost_ = true;
    }
}

void Renderer::setDpi(UINT dpi) {
    dpi_ = dpi;
    if (context_) {
        context_->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    }
    if (swapChain_) {
        releaseTarget();
        if (auto target = createTarget(); !target) {
            lost_ = true;
        }
    }
}

bool Renderer::beginFrame(Color clear) {
    if (lost_ || !context_ || !target_) {
        return false;
    }
    context_->BeginDraw();
    context_->Clear(toD2D(clear));
    drawing_ = true;
    return true;
}

void Renderer::endFrame() {
    if (!drawing_) {
        return;
    }
    drawing_ = false;
    HRESULT hr = context_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        lost_ = true;
        return;
    }
    hr = swapChain_->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        lost_ = true;
    }
}

ID2D1SolidColorBrush* Renderer::brush(Color color) {
    brush_->SetColor(toD2D(color));
    return brush_.get();
}

void Renderer::fillRect(const Rect& rect, Color color, float radius) {
    if (rect.empty() || color.a <= 0.0f) {
        return;
    }
    if (radius > 0.0f) {
        context_->FillRoundedRectangle(D2D1::RoundedRect(toD2D(rect), radius, radius), brush(color));
    } else {
        context_->FillRectangle(toD2D(rect), brush(color));
    }
}

void Renderer::strokeRect(const Rect& rect, Color color, float width, float radius) {
    if (rect.empty() || color.a <= 0.0f) {
        return;
    }
    const Rect inner = rect.inset(width * kStrokeAlignInset);
    if (radius > 0.0f) {
        context_->DrawRoundedRectangle(D2D1::RoundedRect(toD2D(inner), std::max(0.0f, radius - width * kStrokeAlignInset), std::max(0.0f, radius - width * kStrokeAlignInset)), brush(color), width);
    } else {
        context_->DrawRectangle(toD2D(inner), brush(color), width);
    }
}

void Renderer::fillCircle(Point center, float radius, Color color) {
    context_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(center.x, center.y), radius, radius), brush(color));
}

void Renderer::drawLine(Point a, Point b, Color color, float width) {
    context_->DrawLine(D2D1::Point2F(a.x, a.y), D2D1::Point2F(b.x, b.y), brush(color), width);
}

IDWriteTextFormat3* Renderer::format(const TextStyle& style) {
    const auto key = std::make_tuple(static_cast<int>(style.font), static_cast<int>(std::lround(style.size * 4.0f)), style.weight);
    if (auto it = formats_.find(key); it != formats_.end()) {
        return it->second.get();
    }
    const wchar_t* family = style.font == Font::Mono ? L"JetBrains Mono" : style.font == Font::Title ? L"Schibsted Grotesk" : L"Hanken Grotesk";
    DWRITE_FONT_AXIS_VALUE axes[] = {{DWRITE_FONT_AXIS_TAG_WEIGHT, static_cast<float>(style.weight)}};
    winrt::com_ptr<IDWriteTextFormat3> fmt;
    HRESULT hr = writeFactory_->CreateTextFormat(family, fonts_.get(), axes, ARRAYSIZE(axes), style.size, L"en-us", fmt.put());
    if (FAILED(hr)) {
        winrt::com_ptr<IDWriteTextFormat> plain;
        writeFactory_->CreateTextFormat(L"Segoe UI", nullptr, static_cast<DWRITE_FONT_WEIGHT>(style.weight), DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, style.size, L"en-us", plain.put());
        fmt = plain.try_as<IDWriteTextFormat3>();
        if (!fmt) {
            return nullptr;
        }
    }
    fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    winrt::com_ptr<IDWriteInlineObject> sign;
    if (SUCCEEDED(writeFactory_->CreateEllipsisTrimmingSign(fmt.get(), sign.put()))) {
        ellipsis_[key] = sign;
    }
    formats_[key] = fmt;
    return fmt.get();
}

winrt::com_ptr<IDWriteTextLayout> Renderer::createLayout(std::wstring_view text, const TextStyle& style, float maxWidth, float maxHeight, bool wrap) {
    IDWriteTextFormat3* fmt = format(style);
    winrt::com_ptr<IDWriteTextLayout> layout;
    if (fmt == nullptr) {
        return layout;
    }
    if (FAILED(writeFactory_->CreateTextLayout(text.data(), static_cast<UINT32>(text.size()), fmt, std::max(1.0f, maxWidth), std::max(1.0f, maxHeight), layout.put()))) {
        return nullptr;
    }
    layout->SetWordWrapping(wrap ? DWRITE_WORD_WRAPPING_WRAP : DWRITE_WORD_WRAPPING_NO_WRAP);
    if (style.letterSpacing != 0.0f) {
        if (auto l1 = layout.try_as<IDWriteTextLayout1>()) {
            const DWRITE_TEXT_RANGE range{0, static_cast<UINT32>(text.size())};
            l1->SetCharacterSpacing(0.0f, style.letterSpacing, 0.0f, range);
        }
    }
    return layout;
}

Size Renderer::measureText(std::wstring_view text, const TextStyle& style, float maxWidth, bool wrap) {
    auto layout = createLayout(text, style, maxWidth, 10000.0f, wrap);
    if (!layout) {
        return {};
    }
    DWRITE_TEXT_METRICS metrics{};
    layout->GetMetrics(&metrics);
    return {std::ceil(metrics.widthIncludingTrailingWhitespace), std::ceil(metrics.height)};
}

void Renderer::drawLayout(IDWriteTextLayout* layout, Point origin, Color color) {
    if (layout == nullptr) {
        return;
    }
    context_->DrawTextLayout(D2D1::Point2F(origin.x, origin.y), layout, brush(color), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

void Renderer::drawText(std::wstring_view text, const TextStyle& style, const Rect& bounds, Color color, const TextOptions& options) {
    if (text.empty() || bounds.empty()) {
        return;
    }
    auto layout = createLayout(text, style, bounds.w, bounds.h, options.wrap);
    if (!layout) {
        return;
    }
    layout->SetTextAlignment(options.horizontal == Align::Center ? DWRITE_TEXT_ALIGNMENT_CENTER : options.horizontal == Align::End ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    layout->SetParagraphAlignment(options.vertical == Align::Center ? DWRITE_PARAGRAPH_ALIGNMENT_CENTER : options.vertical == Align::End ? DWRITE_PARAGRAPH_ALIGNMENT_FAR : DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    if (options.ellipsis && !options.wrap) {
        const auto key = std::make_tuple(static_cast<int>(style.font), static_cast<int>(std::lround(style.size * 4.0f)), style.weight);
        DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
        auto sign = ellipsis_.find(key);
        layout->SetTrimming(&trimming, sign != ellipsis_.end() ? sign->second.get() : nullptr);
    }
    context_->DrawTextLayout(D2D1::Point2F(bounds.x, bounds.y), layout.get(), brush(color), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT | D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

void Renderer::pushClip(const Rect& rect) {
    context_->PushAxisAlignedClip(toD2D(rect), D2D1_ANTIALIAS_MODE_ALIASED);
}

void Renderer::popClip() {
    context_->PopAxisAlignedClip();
}

void Renderer::pushOpacity(const Rect& rect, float opacity) {
    auto params = D2D1::LayerParameters1(toD2D(rect), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), opacity);
    context_->PushLayer(params, nullptr);
}

void Renderer::popOpacity() {
    context_->PopLayer();
}

void Renderer::fillGeometry(ID2D1Geometry* geometry, Color color, const D2D1_MATRIX_3X2_F& transform) {
    if (geometry == nullptr) {
        return;
    }
    D2D1_MATRIX_3X2_F previous{};
    context_->GetTransform(&previous);
    context_->SetTransform(transform * previous);
    context_->FillGeometry(geometry, brush(color));
    context_->SetTransform(previous);
}

void Renderer::strokeGeometry(ID2D1Geometry* geometry, Color color, float width, const D2D1_MATRIX_3X2_F& transform, bool roundCaps) {
    if (geometry == nullptr) {
        return;
    }
    winrt::com_ptr<ID2D1StrokeStyle> style;
    if (roundCaps) {
        D2D1_STROKE_STYLE_PROPERTIES props = D2D1::StrokeStyleProperties(D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_CAP_STYLE_ROUND, D2D1_LINE_JOIN_ROUND);
        factory_->CreateStrokeStyle(props, nullptr, 0, style.put());
    }
    D2D1_MATRIX_3X2_F previous{};
    context_->GetTransform(&previous);
    context_->SetTransform(transform * previous);
    context_->DrawGeometry(geometry, brush(color), width, style.get());
    context_->SetTransform(previous);
}

void Renderer::drawBitmap(ID2D1Bitmap* bitmap, const Rect& dest, float opacity, float radius) {
    if (bitmap == nullptr) {
        return;
    }
    if (radius > 0.0f) {
        winrt::com_ptr<ID2D1RoundedRectangleGeometry> geometry;
        factory_->CreateRoundedRectangleGeometry(D2D1::RoundedRect(toD2D(dest), radius, radius), geometry.put());
        winrt::com_ptr<ID2D1BitmapBrush> bitmapBrush;
        const auto size = bitmap->GetSize();
        auto props = D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP, D2D1_EXTEND_MODE_CLAMP, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
        auto brushProps = D2D1::BrushProperties(opacity, D2D1::Matrix3x2F::Scale(dest.w / size.width, dest.h / size.height) * D2D1::Matrix3x2F::Translation(dest.x, dest.y));
        if (SUCCEEDED(context_->CreateBitmapBrush(bitmap, props, brushProps, bitmapBrush.put())) && geometry) {
            context_->FillGeometry(geometry.get(), bitmapBrush.get());
            return;
        }
    }
    context_->DrawBitmap(bitmap, toD2D(dest), opacity, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
}

winrt::com_ptr<ID2D1Bitmap1> Renderer::loadPng(std::span<const std::uint8_t> bytes) {
    winrt::com_ptr<ID2D1Bitmap1> result;
    winrt::com_ptr<IWICImagingFactory> wic;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wic.put())))) {
        return result;
    }
    winrt::com_ptr<IWICStream> stream;
    if (FAILED(wic->CreateStream(stream.put())) || FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())))) {
        return result;
    }
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    if (FAILED(wic->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnLoad, decoder.put()))) {
        return result;
    }
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, frame.put()))) {
        return result;
    }
    winrt::com_ptr<IWICFormatConverter> converter;
    if (FAILED(wic->CreateFormatConverter(converter.put())) ||
        FAILED(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
        return result;
    }
    const auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    context_->CreateBitmapFromWicBitmap(converter.get(), &props, result.put());
    return result;
}

}

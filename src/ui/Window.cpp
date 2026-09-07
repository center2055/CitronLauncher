#include "ui/Window.h"

#include "core/Logger.h"
#include "platform/windows/Dpi.h"
#include "ui/Animation.h"

#include <dwmapi.h>
#include <windowsx.h>

#include <cmath>

namespace citron::ui {

namespace {

constexpr wchar_t kClassName[] = L"CitronLauncherWindow";
constexpr UINT WM_APP_DISPATCH = WM_APP + 1;
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT_PTR kTimeoutBase = 100;

}

Window::Window(HINSTANCE instance) : instance_(instance) {}

Window::~Window() {
    if (hwnd_ != nullptr) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
    }
}

Result<void> Window::create(const WindowOptions& options, std::span<const std::uint8_t> bodyFont, std::span<const std::uint8_t> titleFont, std::span<const std::uint8_t> monoFont) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = &Window::wndProc;
    wc.hInstance = instance_;
    wc.hIcon = options.icon;
    wc.hIconSm = options.smallIcon;
    wc.hCursor = nullptr;
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Internal, "register window class", GetLastError(), "The window could not be created."));
    }
    minWidth_ = options.minWidth;
    minHeight_ = options.minHeight;

    const UINT systemDpi = GetDpiForSystem();
    const int width = platform::dipsToPixels(static_cast<float>(options.width), systemDpi);
    const int height = platform::dipsToPixels(static_cast<float>(options.height), systemDpi);
    const DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    const DWORD exStyle = WS_EX_NOREDIRECTIONBITMAP | WS_EX_APPWINDOW;
    hwnd_ = CreateWindowExW(exStyle, kClassName, options.title.c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance_, this);
    if (hwnd_ == nullptr) {
        return std::unexpected(Error::fromWin32(ErrorCategory::Internal, "create window", GetLastError(), "The window could not be created."));
    }
    dpi_ = platform::dpiForWindow(hwnd_);
    if (dpi_ != systemDpi) {
        const int w = platform::dipsToPixels(static_cast<float>(options.width), dpi_);
        const int h = platform::dipsToPixels(static_cast<float>(options.height), dpi_);
        SetWindowPos(hwnd_, nullptr, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    {
        RECT rc{};
        GetWindowRect(hwnd_, &rc);
        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info)) {
            const int w = rc.right - rc.left;
            const int h = rc.bottom - rc.top;
            const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - w) / 2;
            const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - h) / 2;
            SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    const DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
    const MARGINS margins{1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd_, &margins);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    updateClientSize();
    if (auto init = renderer_.initialize(hwnd_, dpi_, bodyFont, titleFont, monoFont); !init) {
        return init;
    }
    if (options.maximized) {
        ShowWindow(hwnd_, SW_SHOWMAXIMIZED);
        ShowWindow(hwnd_, SW_HIDE);
    }
    return {};
}

void Window::show() {
    ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_SHOWMAXIMIZED : SW_SHOW);
    UpdateWindow(hwnd_);
}

int Window::runMessageLoop() {
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void Window::setRoot(Element* root) {
    root_ = root;
    if (root_ != nullptr) {
        root_->setHost(this);
        layout();
    }
}

void Window::setTheme(const Theme& theme) {
    theme_ = theme;
    const BOOL dark = theme.dark ? TRUE : FALSE;
    if (hwnd_ != nullptr) {
        DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    }
    invalidate();
}

void Window::setDispatcher(Dispatcher* dispatcher) {
    dispatcher_ = dispatcher;
    if (dispatcher_ != nullptr) {
        HWND hwnd = hwnd_;
        dispatcher_->setWakeup([hwnd] { PostMessageW(hwnd, WM_APP_DISPATCH, 0, 0); });
    }
}

void Window::close() {
    if (hwnd_ != nullptr) {
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
    }
}

void Window::minimize() {
    ShowWindow(hwnd_, SW_MINIMIZE);
}

void Window::toggleMaximize() {
    ShowWindow(hwnd_, IsZoomed(hwnd_) ? SW_RESTORE : SW_MAXIMIZE);
}

bool Window::maximized() const {
    return hwnd_ != nullptr && IsZoomed(hwnd_);
}

void Window::windowRect(int& width, int& height) const {
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (GetWindowPlacement(hwnd_, &placement)) {
        const RECT& rc = placement.rcNormalPosition;
        width = static_cast<int>(std::lround(platform::pixelsToDips(rc.right - rc.left, dpi_)));
        height = static_cast<int>(std::lround(platform::pixelsToDips(rc.bottom - rc.top, dpi_)));
    }
}

void Window::setTimeout(unsigned id, unsigned ms, std::function<void()> fn) {
    timeouts_[id] = std::move(fn);
    SetTimer(hwnd_, kTimeoutBase + id, ms, nullptr);
}

void Window::cancelTimeout(unsigned id) {
    timeouts_.erase(id);
    KillTimer(hwnd_, kTimeoutBase + id);
}

void Window::invalidate() {
    if (hwnd_ != nullptr && !painting_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void Window::requestFrame() {
    requestFrameIn(16);
}

void Window::requestFrameIn(unsigned delayMs) {
    if (hwnd_ == nullptr) {
        return;
    }
    const double due = now() + static_cast<double>(delayMs);
    if (frameTimer_ && due >= frameDue_) {
        return;
    }
    frameTimer_ = true;
    frameDue_ = due;
    SetTimer(hwnd_, kFrameTimer, std::max(1u, delayMs), nullptr);
}

void Window::requestFocus(Element* element) {
    if (focused_ == element) {
        return;
    }
    Element* previous = focused_;
    focused_ = element;
    if (previous != nullptr) {
        previous->onFocusChanged(false);
    }
    if (focused_ != nullptr) {
        focused_->onFocusChanged(true);
    }
    invalidate();
}

Size Window::measureText(std::wstring_view text, const TextStyle& style, float maxWidth, bool wrap) {
    return renderer_.measureText(text, style, maxWidth, wrap);
}

double Window::now() const {
    return nowMs();
}

void Window::elementDestroyed(Element* element) {
    if (hovered_ == element) {
        hovered_ = nullptr;
    }
    if (pressed_ == element) {
        pressed_ = nullptr;
    }
    if (focused_ == element) {
        focused_ = nullptr;
    }
}

void Window::layout() {
    if (root_ == nullptr) {
        return;
    }
    root_->measure(client_);
    root_->arrange({0.0f, 0.0f, client_.w, client_.h});
}

void Window::updateClientSize() {
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    client_ = {platform::pixelsToDips(rc.right - rc.left, dpi_), platform::pixelsToDips(rc.bottom - rc.top, dpi_)};
}

void Window::render() {
    if (root_ == nullptr || IsIconic(hwnd_)) {
        return;
    }
    if (renderer_.lost()) {
        if (auto recovered = renderer_.recover(); !recovered) {
            log::error("graphics recovery failed: {}", recovered.error().summary());
            return;
        }
    }
    painting_ = true;
    if (renderer_.beginFrame(theme_.bg)) {
        RenderContext ctx{renderer_, theme_, now()};
        root_->render(ctx);
        renderer_.endFrame();
    }
    painting_ = false;
    if (!firstFrame_) {
        firstFrame_ = true;
        log::info("first frame at {:.1f} ms", log::elapsedMs());
        if (onFirstFrame_) {
            onFirstFrame_();
        }
    }
}

Point Window::toDips(LPARAM lParam) const {
    return {platform::pixelsToDips(GET_X_LPARAM(lParam), dpi_), platform::pixelsToDips(GET_Y_LPARAM(lParam), dpi_)};
}

Point Window::screenToDips(LPARAM lParam) const {
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hwnd_, &pt);
    return {platform::pixelsToDips(pt.x, dpi_), platform::pixelsToDips(pt.y, dpi_)};
}

void Window::clearHover() {
    if (hovered_ != nullptr) {
        hovered_->setHovered(false);
        hovered_ = nullptr;
    }
}

void Window::applyCursor() {
    Cursor cursor = hovered_ != nullptr ? hovered_->cursor() : Cursor::Arrow;
    const wchar_t* id = IDC_ARROW;
    if (cursor == Cursor::Hand) {
        id = IDC_HAND;
    } else if (cursor == Cursor::IBeam) {
        id = IDC_IBEAM;
    }
    SetCursor(LoadCursorW(nullptr, id));
}

void Window::routeMouseMove(Point point) {
    if (pressed_ != nullptr) {
        pressed_->onMouseMove(point);
        return;
    }
    Element* target = root_ != nullptr ? root_->hitTest(point) : nullptr;
    if (target != hovered_) {
        if (hovered_ != nullptr) {
            hovered_->setHovered(false);
        }
        hovered_ = target;
        if (hovered_ != nullptr) {
            hovered_->setHovered(true);
        }
    }
    if (hovered_ != nullptr) {
        hovered_->onMouseMove(point);
    }
}

void Window::routeMouseDown(Point point, int button) {
    keyboardFocus_ = false;
    Element* target = root_ != nullptr ? root_->hitTest(point) : nullptr;
    if (target != hovered_) {
        clearHover();
        hovered_ = target;
        if (hovered_ != nullptr) {
            hovered_->setHovered(true);
        }
    }
    if (target == nullptr) {
        requestFocus(nullptr);
        return;
    }
    SetCapture(hwnd_);
    pressed_ = target;
    pressedButton_ = button;
    pressed_->setPressed(true);
    if (target->focusable()) {
        requestFocus(target);
    } else {
        bool insideFocused = false;
        for (Element* e = target; e != nullptr; e = e->parent()) {
            if (e == focused_) {
                insideFocused = true;
                break;
            }
        }
        if (!insideFocused) {
            requestFocus(nullptr);
        }
    }
    target->onMouseDown(point, button);
    invalidate();
}

void Window::routeMouseUp(Point point, int button) {
    if (GetCapture() == hwnd_) {
        ReleaseCapture();
    }
    Element* pressed = pressed_;
    pressed_ = nullptr;
    if (pressed != nullptr) {
        pressed->setPressed(false);
        pressed->onMouseUp(point, button);
    }
    routeMouseMove(point);
    invalidate();
}

void Window::routeWheel(Point point, float delta) {
    Element* target = root_ != nullptr ? root_->hitTest(point) : nullptr;
    for (Element* e = target; e != nullptr; e = e->parent()) {
        if (e->onWheel(point, delta)) {
            return;
        }
    }
}

void Window::focusNext(bool backward) {
    if (root_ == nullptr) {
        return;
    }
    std::vector<Element*> order;
    root_->collectFocusable(order);
    if (order.empty()) {
        return;
    }
    keyboardFocus_ = true;
    size_t index = 0;
    bool found = false;
    for (size_t i = 0; i < order.size(); ++i) {
        if (order[i] == focused_) {
            index = i;
            found = true;
            break;
        }
    }
    Element* next = nullptr;
    if (!found) {
        next = backward ? order.back() : order.front();
    } else if (backward) {
        next = order[(index + order.size() - 1) % order.size()];
    } else {
        next = order[(index + 1) % order.size()];
    }
    requestFocus(next);
}

int Window::frameThickness() const {
    return platform::systemMetricForDpi(SM_CXFRAME, dpi_) + platform::systemMetricForDpi(SM_CXPADDEDBORDER, dpi_);
}

LRESULT Window::hitTestNonClient(LPARAM lParam) {
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    RECT rc{};
    GetWindowRect(hwnd_, &rc);
    if (!IsZoomed(hwnd_)) {
        const int frame = frameThickness();
        const bool left = pt.x < rc.left + frame;
        const bool right = pt.x >= rc.right - frame;
        const bool top = pt.y < rc.top + frame;
        const bool bottom = pt.y >= rc.bottom - frame;
        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
    }
    if (captionHitTest_) {
        return captionHitTest_(screenToDips(lParam));
    }
    return HTCLIENT;
}

LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<Window*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self == nullptr) {
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return self->handle(msg, wParam, lParam);
}

LRESULT Window::handle(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCCALCSIZE: {
        if (wParam == FALSE) {
            return 0;
        }
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
        if (IsZoomed(hwnd_)) {
            const int frame = frameThickness();
            params->rgrc[0].left += frame;
            params->rgrc[0].top += frame;
            params->rgrc[0].right -= frame;
            params->rgrc[0].bottom -= frame;
        }
        return 0;
    }
    case WM_NCHITTEST:
        return hitTestNonClient(lParam);
    case WM_NCMOUSEMOVE:
        if (wParam == HTMAXBUTTON) {
            if (!trackingNc_) {
                TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE | TME_NONCLIENT, hwnd_, 0};
                TrackMouseEvent(&tme);
                trackingNc_ = true;
            }
            routeMouseMove(screenToDips(lParam));
            return 0;
        }
        clearHover();
        invalidate();
        break;
    case WM_NCMOUSELEAVE:
        trackingNc_ = false;
        if (pressed_ == nullptr) {
            clearHover();
            invalidate();
        }
        break;
    case WM_NCLBUTTONDOWN:
        if (wParam == HTMAXBUTTON) {
            routeMouseDown(screenToDips(lParam), 0);
            return 0;
        }
        break;
    case WM_NCLBUTTONUP:
        if (wParam == HTMAXBUTTON) {
            routeMouseUp(screenToDips(lParam), 0);
            return 0;
        }
        break;
    case WM_NCRBUTTONUP:
        if (wParam == HTCAPTION) {
            POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HMENU menu = GetSystemMenu(hwnd_, FALSE);
            if (menu != nullptr) {
                const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
                if (cmd != 0) {
                    SendMessageW(hwnd_, WM_SYSCOMMAND, static_cast<WPARAM>(cmd), 0);
                }
            }
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = platform::dipsToPixels(static_cast<float>(minWidth_), dpi_);
        info->ptMinTrackSize.y = platform::dipsToPixels(static_cast<float>(minHeight_), dpi_);
        return 0;
    }
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wParam);
        const auto* rc = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd_, nullptr, rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
        renderer_.setDpi(dpi_);
        updateClientSize();
        layout();
        invalidate();
        return 0;
    }
    case WM_SIZE: {
        if (wParam == SIZE_MINIMIZED) {
            return 0;
        }
        renderer_.resize(LOWORD(lParam), HIWORD(lParam));
        updateClientSize();
        layout();
        if (onResize_) {
            onResize_();
        }
        render();
        return 0;
    }
    case WM_PAINT: {
        ValidateRect(hwnd_, nullptr);
        render();
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_TIMER:
        if (wParam == kFrameTimer) {
            KillTimer(hwnd_, kFrameTimer);
            frameTimer_ = false;
            render();
            return 0;
        }
        if (wParam >= kTimeoutBase) {
            const auto id = static_cast<unsigned>(wParam - kTimeoutBase);
            KillTimer(hwnd_, wParam);
            auto it = timeouts_.find(id);
            if (it != timeouts_.end()) {
                auto fn = std::move(it->second);
                timeouts_.erase(it);
                if (fn) {
                    fn();
                }
            }
            return 0;
        }
        break;
    case WM_APP_DISPATCH:
        if (dispatcher_ != nullptr) {
            dispatcher_->drain();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (!tracking_) {
            TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
            TrackMouseEvent(&tme);
            tracking_ = true;
        }
        routeMouseMove(toDips(lParam));
        return 0;
    case WM_MOUSELEAVE:
        tracking_ = false;
        if (pressed_ == nullptr) {
            clearHover();
            invalidate();
        }
        return 0;
    case WM_LBUTTONDOWN:
        routeMouseDown(toDips(lParam), 0);
        return 0;
    case WM_LBUTTONUP:
        routeMouseUp(toDips(lParam), 0);
        return 0;
    case WM_RBUTTONDOWN:
        routeMouseDown(toDips(lParam), 1);
        return 0;
    case WM_RBUTTONUP:
        routeMouseUp(toDips(lParam), 1);
        return 0;
    case WM_LBUTTONDBLCLK: {
        const Point p = toDips(lParam);
        if (Element* target = root_ != nullptr ? root_->hitTest(p) : nullptr) {
            target->onDoubleClick(p);
        }
        routeMouseDown(p, 0);
        return 0;
    }
    case WM_MOUSEWHEEL:
        routeWheel(screenToDips(lParam), static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lParam) == HTCLIENT) {
            applyCursor();
            return TRUE;
        }
        break;
    case WM_KEYDOWN: {
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wParam == VK_TAB) {
            focusNext(shift);
            return 0;
        }
        if (wParam == VK_ESCAPE || wParam == VK_RETURN || wParam == VK_SPACE || wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT) {
            keyboardFocus_ = true;
        }
        for (Element* e = focused_; e != nullptr; e = e->parent()) {
            if (e->onKeyDown(static_cast<unsigned>(wParam), ctrl, shift)) {
                invalidate();
                return 0;
            }
        }
        if (root_ != nullptr && root_->onKeyDown(static_cast<unsigned>(wParam), ctrl, shift)) {
            invalidate();
            return 0;
        }
        break;
    }
    case WM_CHAR:
        if (focused_ != nullptr && wParam >= 0x20) {
            focused_->onChar(static_cast<wchar_t>(wParam));
            return 0;
        }
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        invalidate();
        break;
    case WM_CLOSE:
        if (onClosing_) {
            onClosing_();
        }
        DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        hwnd_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

}

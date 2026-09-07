#pragma once

#include "core/Error.h"
#include "core/TaskScheduler.h"
#include "ui/Renderer.h"
#include "ui/Theme.h"
#include "ui/controls/Element.h"

#include <windows.h>

#include <functional>
#include <map>
#include <span>
#include <string>

namespace citron::ui {

struct WindowOptions {
    int width = 880;
    int height = 540;
    int minWidth = 760;
    int minHeight = 480;
    bool maximized = false;
    std::wstring title;
    HICON icon = nullptr;
    HICON smallIcon = nullptr;
};

class Window : public Host {
public:
    explicit Window(HINSTANCE instance);
    ~Window() override;

    Result<void> create(const WindowOptions& options, std::span<const std::uint8_t> bodyFont, std::span<const std::uint8_t> titleFont, std::span<const std::uint8_t> monoFont);
    void show();
    int runMessageLoop();

    void setRoot(Element* root);
    void setTheme(const Theme& theme);
    void setDispatcher(Dispatcher* dispatcher);
    void setCaptionHitTest(std::function<int(Point)> fn) { captionHitTest_ = std::move(fn); }
    void setOnResize(std::function<void()> fn) { onResize_ = std::move(fn); }
    void setOnClosing(std::function<void()> fn) { onClosing_ = std::move(fn); }
    void setOnFirstFrame(std::function<void()> fn) { onFirstFrame_ = std::move(fn); }

    HWND hwnd() const { return hwnd_; }
    void close();
    void minimize();
    void toggleMaximize();
    bool maximized() const;
    Size clientSize() const { return client_; }
    UINT dpi() const { return dpi_; }
    void windowRect(int& width, int& height) const;
    void setTimeout(unsigned id, unsigned ms, std::function<void()> fn);
    void cancelTimeout(unsigned id);
    void focusNext(bool backward);
    void layout();

    void invalidate() override;
    void requestFrame() override;
    void requestFrameIn(unsigned delayMs) override;
    void requestFocus(Element* element) override;
    Element* focusedElement() const override { return focused_; }
    const Theme& theme() const override { return theme_; }
    Size measureText(std::wstring_view text, const TextStyle& style, float maxWidth, bool wrap) override;
    Renderer& renderer() override { return renderer_; }
    double now() const override;
    bool keyboardFocusVisible() const override { return keyboardFocus_; }
    void elementDestroyed(Element* element) override;

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT msg, WPARAM wParam, LPARAM lParam);
    void render();
    void updateClientSize();
    Point toDips(LPARAM lParam) const;
    Point screenToDips(LPARAM lParam) const;
    void routeMouseMove(Point point);
    void routeMouseDown(Point point, int button);
    void routeMouseUp(Point point, int button);
    void routeWheel(Point point, float delta);
    void clearHover();
    void applyCursor();
    LRESULT hitTestNonClient(LPARAM lParam);
    int frameThickness() const;

    HINSTANCE instance_;
    HWND hwnd_ = nullptr;
    Renderer renderer_;
    Theme theme_ = Theme::darkTheme();
    Element* root_ = nullptr;
    Element* hovered_ = nullptr;
    Element* pressed_ = nullptr;
    Element* focused_ = nullptr;
    Dispatcher* dispatcher_ = nullptr;
    std::function<int(Point)> captionHitTest_;
    std::function<void()> onResize_;
    std::function<void()> onClosing_;
    std::function<void()> onFirstFrame_;
    std::map<unsigned, std::function<void()>> timeouts_;
    UINT dpi_ = 96;
    Size client_;
    int minWidth_ = 760;
    int minHeight_ = 480;
    bool frameTimer_ = false;
    double frameDue_ = 0.0;
    bool tracking_ = false;
    bool trackingNc_ = false;
    bool keyboardFocus_ = false;
    bool firstFrame_ = false;
    bool painting_ = false;
    int pressedButton_ = -1;
};

}

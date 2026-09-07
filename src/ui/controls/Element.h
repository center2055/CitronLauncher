#pragma once

#include "ui/Geometry.h"
#include "ui/Renderer.h"
#include "ui/Theme.h"

#include <memory>
#include <string>
#include <vector>

namespace citron::ui {

class Element;

enum class Cursor {
    Arrow,
    Hand,
    IBeam,
};

class Host {
public:
    virtual ~Host() = default;
    virtual void invalidate() = 0;
    virtual void requestFrame() = 0;
    virtual void requestFrameIn(unsigned delayMs) = 0;
    virtual void requestFocus(Element* element) = 0;
    virtual Element* focusedElement() const = 0;
    virtual const Theme& theme() const = 0;
    virtual Size measureText(std::wstring_view text, const TextStyle& style, float maxWidth = 1e6f, bool wrap = false) = 0;
    virtual Renderer& renderer() = 0;
    virtual double now() const = 0;
    virtual bool keyboardFocusVisible() const = 0;
    virtual void elementDestroyed(Element* element) = 0;
};

struct RenderContext {
    Renderer& r;
    const Theme& theme;
    double now;
};

class Element {
public:
    virtual ~Element();

    virtual Size measure(const Size& available);
    virtual void arrange(const Rect& bounds);
    virtual void render(RenderContext& ctx);
    virtual Element* hitTest(Point point);
    virtual bool interactive() const { return false; }
    virtual bool focusable() const { return false; }
    virtual Cursor cursor() const { return Cursor::Arrow; }

    virtual void onMouseEnter() {}
    virtual void onMouseLeave() {}
    virtual void onMouseMove(Point) {}
    virtual void onMouseDown(Point, int) {}
    virtual void onMouseUp(Point, int) {}
    virtual void onDoubleClick(Point) {}
    virtual bool onWheel(Point, float) { return false; }
    virtual bool onKeyDown(unsigned vk, bool ctrl, bool shift) { (void)vk; (void)ctrl; (void)shift; return false; }
    virtual void onChar(wchar_t) {}
    virtual void onFocusChanged(bool) {}

    Element* addChild(std::unique_ptr<Element> child);
    template <typename T>
    T* add(std::unique_ptr<T> child) {
        T* raw = child.get();
        addChild(std::move(child));
        return raw;
    }
    void clearChildren();
    const std::vector<std::unique_ptr<Element>>& children() const { return children_; }
    Element* parent() const { return parent_; }
    void setHost(Host* host);
    Host* host() const { return host_; }

    const Rect& bounds() const { return bounds_; }
    void setBounds(const Rect& bounds) { bounds_ = bounds; }
    bool visible() const { return visible_; }
    void setVisible(bool visible);
    bool enabled() const { return enabled_; }
    void setEnabled(bool enabled);
    bool hovered() const { return hovered_; }
    bool pressed() const { return pressed_; }
    bool focused() const;
    void setHovered(bool hovered);
    void setPressed(bool pressed);
    void invalidate();
    void collectFocusable(std::vector<Element*>& out);
    const std::wstring& tooltip() const { return tooltip_; }
    void setTooltip(std::wstring text) { tooltip_ = std::move(text); }

protected:
    Rect bounds_;
    Host* host_ = nullptr;

private:
    Element* parent_ = nullptr;
    std::vector<std::unique_ptr<Element>> children_;
    bool visible_ = true;
    bool enabled_ = true;
    bool hovered_ = false;
    bool pressed_ = false;
    std::wstring tooltip_;
};

class Spacer : public Element {
public:
    explicit Spacer(Size size) : size_(size) {}
    Size measure(const Size&) override { return size_; }

private:
    Size size_;
};

}

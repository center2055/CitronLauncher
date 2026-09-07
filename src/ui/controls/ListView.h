#pragma once

#include "ui/controls/Element.h"

#include <functional>

namespace citron::ui {

class ListRow : public Element {
public:
    void setOnActivate(std::function<void()> fn) { onActivate_ = std::move(fn); }
    void setSelected(bool selected);
    bool selected() const { return selected_; }
    void setRowHeight(float height) { height_ = height; }
    void setRowPadding(float horizontal, float vertical) { padX_ = horizontal; padY_ = vertical; }
    void setSelectable(bool selectable) { selectable_ = selectable; }
    void setDimmed(bool dimmed) { dimmed_ = dimmed; }

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return selectable_; }
    void onMouseUp(Point point, int button) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;

protected:
    virtual void renderContent(RenderContext& ctx) = 0;
    Rect contentRect() const { return bounds_.inset(padX_, padY_, padX_, padY_); }

private:
    std::function<void()> onActivate_;
    bool selected_ = false;
    bool selectable_ = true;
    bool dimmed_ = false;
    float height_ = 34.0f;
    float padX_ = 12.0f;
    float padY_ = 6.0f;
};

class ListView : public Element {
public:
    explicit ListView(float gap = 2.0f);

    void setGap(float gap) { gap_ = gap; }
    void setPadding(float left, float top, float right, float bottom);

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;
    bool interactive() const override { return true; }

private:
    float gap_;
    float padLeft_ = 0.0f, padTop_ = 0.0f, padRight_ = 0.0f, padBottom_ = 0.0f;
};

}

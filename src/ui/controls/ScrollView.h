#pragma once

#include "ui/Animation.h"
#include "ui/controls/Element.h"

namespace citron::ui {

class ScrollView : public Element {
public:
    ScrollView();

    void setContent(std::unique_ptr<Element> content);
    Element* content() const { return content_; }
    void setPadding(float left, float top, float right, float bottom);
    float offset() const { return offset_; }
    void scrollTo(float offset);
    void ensureVisible(const Rect& rect);

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;
    Element* hitTest(Point point) override;
    bool interactive() const override { return true; }
    bool onWheel(Point point, float delta) override;
    void onMouseDown(Point point, int button) override;
    void onMouseMove(Point point) override;
    void onMouseUp(Point point, int button) override;

private:
    float maxOffset() const;
    Rect thumbRect() const;
    void applyOffset();

    Element* content_ = nullptr;
    float offset_ = 0.0f;
    Animated scroll_{0.0f, 160.0};
    float contentHeight_ = 0.0f;
    float padLeft_ = 0.0f, padTop_ = 0.0f, padRight_ = 0.0f, padBottom_ = 0.0f;
    bool dragging_ = false;
    float dragStartY_ = 0.0f;
    float dragStartOffset_ = 0.0f;
};

}

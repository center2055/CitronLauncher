#pragma once

#include "ui/Icons.h"
#include "ui/controls/Element.h"

#include <functional>

namespace citron::ui {

enum class IconButtonKind {
    Caption,
    CaptionClose,
    Plain,
};

class IconButton : public Element {
public:
    IconButton(Icon icon, IconButtonKind kind, std::function<void()> onClick);

    void setIcon(Icon icon);
    void setSize(Size size) { size_ = size; }
    void setIconSize(float size) { iconSize_ = size; }
    void setSpinning(bool spinning);
    void setStroke(float width) { stroke_ = width; }
    void click();

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return kind_ == IconButtonKind::Plain; }
    void onMouseUp(Point point, int button) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;

private:
    Icon icon_;
    IconButtonKind kind_;
    std::function<void()> onClick_;
    Size size_{36.0f, 30.0f};
    float iconSize_ = 12.0f;
    float stroke_ = 1.4f;
    bool spinning_ = false;
};

}

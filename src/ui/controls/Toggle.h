#pragma once

#include "ui/Animation.h"
#include "ui/controls/Element.h"

#include <functional>

namespace citron::ui {

class Toggle : public Element {
public:
    Toggle(bool on, std::function<void(bool)> onChange);

    void setOn(bool on, bool animate = true);
    bool on() const { return on_; }

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return true; }
    void onMouseUp(Point point, int button) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;

private:
    void flip();

    bool on_;
    std::function<void(bool)> onChange_;
    Animated knob_;
};

}

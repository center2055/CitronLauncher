#pragma once

#include "ui/controls/Button.h"
#include "ui/controls/Element.h"

#include <functional>
#include <string>

namespace citron::ui {

class Toast : public Element {
public:
    Toast();

    void show(std::wstring text, bool error, std::wstring actionLabel, std::function<void()> action, double now);
    void hide();
    bool showing() const { return visible(); }
    double shownAt() const { return shownAt_; }
    bool error() const { return error_; }

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }

private:
    std::wstring text_;
    bool error_ = false;
    double shownAt_ = 0.0;
    Button* action_ = nullptr;
};

}

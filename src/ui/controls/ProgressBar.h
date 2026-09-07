#pragma once

#include "ui/Animation.h"
#include "ui/controls/Element.h"

namespace citron::ui {

class ProgressBar : public Element {
public:
    ProgressBar();

    void setFraction(float fraction, bool animate = true);
    float fraction() const { return target_; }
    void setWidth(float width) { width_ = width; }
    void setError(bool error);

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;

private:
    Animated value_;
    float target_ = 0.0f;
    float width_ = 0.0f;
    bool error_ = false;
};

}

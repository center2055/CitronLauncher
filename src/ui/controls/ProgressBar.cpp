#include "ui/controls/ProgressBar.h"

#include <algorithm>

namespace citron::ui {

ProgressBar::ProgressBar() : value_(0.0f, 300.0) {}

void ProgressBar::setFraction(float fraction, bool animate) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    if (fraction == target_) {
        return;
    }
    target_ = fraction;
    value_.set(fraction, !animate || fraction < value_.value());
    invalidate();
    if (host_ != nullptr && value_.active()) {
        host_->requestFrame();
    }
}

void ProgressBar::setError(bool error) {
    if (error_ != error) {
        error_ = error;
        invalidate();
    }
}

Size ProgressBar::measure(const Size& available) {
    return {width_ > 0.0f ? width_ : available.w, 4.0f};
}

void ProgressBar::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    if (value_.step(ctx.now)) {
        host_->requestFrame();
    }
    ctx.r.fillRect(bounds_, t.border, 2.0f);
    const float w = bounds_.w * value_.value();
    if (w > 0.0f) {
        ctx.r.fillRect({bounds_.x, bounds_.y, w, bounds_.h}, error_ ? t.danger : t.accent, 2.0f);
    }
}

}

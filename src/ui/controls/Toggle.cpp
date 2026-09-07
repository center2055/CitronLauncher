#include "ui/controls/Toggle.h"

namespace citron::ui {

Toggle::Toggle(bool on, std::function<void(bool)> onChange) : on_(on), onChange_(std::move(onChange)), knob_(on ? 1.0f : 0.0f, 200.0) {}

void Toggle::setOn(bool on, bool animate) {
    if (on_ == on) {
        return;
    }
    on_ = on;
    knob_.set(on ? 1.0f : 0.0f, !animate);
    invalidate();
    if (animate && host_ != nullptr) {
        host_->requestFrame();
    }
}

void Toggle::flip() {
    if (!enabled()) {
        return;
    }
    setOn(!on_);
    if (onChange_) {
        onChange_(on_);
    }
}

Size Toggle::measure(const Size&) {
    return {40.0f, 22.0f};
}

void Toggle::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    if (knob_.step(ctx.now)) {
        host_->requestFrame();
    }
    const float k = knob_.value();
    Color track = mix(t.toggleOff, t.accent, k);
    Color knob = mix(t.knobOff, t.accentInk, k);
    if (!enabled()) {
        track = t.dark ? Color::white(0.06f) : Color::black(0.06f);
        knob = t.textFaint;
    }
    ctx.r.fillRect(bounds_, track, 11.0f);
    const float x = bounds_.x + 2.0f + 18.0f * k;
    ctx.r.fillCircle({x + 9.0f, bounds_.y + 11.0f}, 9.0f, knob);
    if (focused() && host_->keyboardFocusVisible()) {
        ctx.r.strokeRect(bounds_.inset(-2.0f), t.textHi, 2.0f, 13.0f);
    }
}

void Toggle::onMouseUp(Point point, int button) {
    if (button == 0 && bounds_.contains(point)) {
        flip();
    }
}

bool Toggle::onKeyDown(unsigned vk, bool, bool) {
    if (vk == VK_SPACE || vk == VK_RETURN) {
        flip();
        return true;
    }
    return false;
}

}

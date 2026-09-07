#include "ui/controls/Toast.h"

namespace citron::ui {

namespace {
const TextStyle kToastStyle{Font::Body, 13.0f, 600, 0.0f};
}

Toast::Toast() {
    setVisible(false);
}

void Toast::show(std::wstring text, bool error, std::wstring actionLabel, std::function<void()> action, double now) {
    clearChildren();
    action_ = nullptr;
    text_ = std::move(text);
    error_ = error;
    shownAt_ = now;
    if (!actionLabel.empty()) {
        auto button = std::make_unique<Button>(std::move(actionLabel), ButtonKind::Ghost, std::move(action));
        button->setHeight(24.0f);
        button->setPadding(8.0f, 8.0f);
        button->setFontSize(12.0f);
        button->setRadius(5.0f);
        action_ = add(std::move(button));
    }
    setVisible(true);
    invalidate();
}

void Toast::hide() {
    setVisible(false);
}

Size Toast::measure(const Size&) {
    float width = 32.0f;
    if (host_ != nullptr) {
        width += host_->measureText(text_, kToastStyle).w;
    }
    if (action_ != nullptr) {
        width += 10.0f + action_->measure({}).w;
    }
    return {width, action_ != nullptr ? 44.0f : 40.0f};
}

void Toast::arrange(const Rect& bounds) {
    bounds_ = bounds;
    if (action_ != nullptr) {
        const Size s = action_->measure({});
        action_->arrange({bounds.right() - 16.0f - s.w, bounds.y + (bounds.h - s.h) / 2.0f, s.w, s.h});
    }
}

void Toast::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    ctx.r.fillRect(bounds_, t.raised, 8.0f);
    ctx.r.strokeRect(bounds_, error_ ? t.danger.withAlpha(0.5f) : t.border2, 1.0f, 8.0f);
    float textRight = bounds_.right() - 16.0f;
    if (action_ != nullptr) {
        textRight = action_->bounds().x - 10.0f;
        if (ctx.theme.dark) {
            ctx.r.fillRect(action_->bounds(), Color::white(0.08f), 5.0f);
        } else {
            ctx.r.fillRect(action_->bounds(), Color::black(0.06f), 5.0f);
        }
    }
    ctx.r.drawText(text_, kToastStyle, {bounds_.x + 16.0f, bounds_.y, textRight - bounds_.x - 16.0f, bounds_.h}, t.textHi, {Align::Start, Align::Center, true, false});
    Element::render(ctx);
}

}

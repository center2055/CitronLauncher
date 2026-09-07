#include "ui/controls/ScrollView.h"

#include <cmath>

namespace citron::ui {

namespace {
constexpr float kBarWidth = 6.0f;
constexpr float kBarMargin = 3.0f;
}

ScrollView::ScrollView() = default;

void ScrollView::setContent(std::unique_ptr<Element> content) {
    clearChildren();
    content_ = add(std::move(content));
}

void ScrollView::setPadding(float left, float top, float right, float bottom) {
    padLeft_ = left;
    padTop_ = top;
    padRight_ = right;
    padBottom_ = bottom;
}

float ScrollView::maxOffset() const {
    return std::max(0.0f, contentHeight_ + padTop_ + padBottom_ - bounds_.h);
}

void ScrollView::scrollTo(float offset) {
    const float clamped = std::clamp(offset, 0.0f, maxOffset());
    if (std::fabs(clamped - offset_) > 0.01f) {
        offset_ = clamped;
        if (content_ != nullptr) {
            arrange(bounds_);
        }
        invalidate();
    }
}

void ScrollView::ensureVisible(const Rect& rect) {
    if (rect.y < bounds_.y) {
        scrollTo(offset_ - (bounds_.y - rect.y));
    } else if (rect.bottom() > bounds_.bottom()) {
        scrollTo(offset_ + (rect.bottom() - bounds_.bottom()));
    }
}

Size ScrollView::measure(const Size& available) {
    if (content_ == nullptr) {
        return {};
    }
    const Size s = content_->measure({std::max(0.0f, available.w - padLeft_ - padRight_), 1e6f});
    return {s.w + padLeft_ + padRight_, std::min(available.h, s.h + padTop_ + padBottom_)};
}

void ScrollView::arrange(const Rect& bounds) {
    bounds_ = bounds;
    if (content_ == nullptr) {
        return;
    }
    const float width = std::max(0.0f, bounds.w - padLeft_ - padRight_);
    const Size s = content_->measure({width, 1e6f});
    contentHeight_ = s.h;
    offset_ = std::clamp(offset_, 0.0f, maxOffset());
    content_->arrange({bounds.x + padLeft_, bounds.y + padTop_ - offset_, width, s.h});
}

void ScrollView::render(RenderContext& ctx) {
    ctx.r.pushClip(bounds_);
    Element::render(ctx);
    ctx.r.popClip();
    if (maxOffset() > 0.0f) {
        const Rect thumb = thumbRect();
        const bool active = hovered() || dragging_;
        ctx.r.fillRect(thumb, active ? ctx.theme.border3 : ctx.theme.border2, kBarWidth / 2.0f);
    }
}

Rect ScrollView::thumbRect() const {
    const float track = bounds_.h - 2.0f * kBarMargin;
    const float total = contentHeight_ + padTop_ + padBottom_;
    const float ratio = total > 0.0f ? bounds_.h / total : 1.0f;
    const float length = std::max(24.0f, track * ratio);
    const float travel = track - length;
    const float pos = maxOffset() > 0.0f ? offset_ / maxOffset() : 0.0f;
    return {bounds_.right() - kBarWidth - kBarMargin, bounds_.y + kBarMargin + travel * pos, kBarWidth, length};
}

Element* ScrollView::hitTest(Point point) {
    if (!visible() || !bounds_.contains(point)) {
        return nullptr;
    }
    if (maxOffset() > 0.0f && thumbRect().inset(-4.0f, 0.0f, -2.0f, 0.0f).contains(point)) {
        return this;
    }
    if (content_ != nullptr) {
        if (Element* hit = content_->hitTest(point)) {
            return hit;
        }
    }
    return this;
}

bool ScrollView::onWheel(Point, float delta) {
    if (maxOffset() <= 0.0f) {
        return false;
    }
    scrollTo(offset_ - delta * 48.0f);
    return true;
}

void ScrollView::onMouseDown(Point point, int button) {
    if (button != 0 || maxOffset() <= 0.0f) {
        return;
    }
    const Rect thumb = thumbRect();
    if (thumb.inset(-4.0f, 0.0f, -2.0f, 0.0f).contains(point)) {
        dragging_ = true;
        dragStartY_ = point.y;
        dragStartOffset_ = offset_;
    }
}

void ScrollView::onMouseMove(Point point) {
    if (!dragging_) {
        return;
    }
    const float track = bounds_.h - 2.0f * kBarMargin - thumbRect().h;
    if (track <= 0.0f) {
        return;
    }
    scrollTo(dragStartOffset_ + (point.y - dragStartY_) / track * maxOffset());
}

void ScrollView::onMouseUp(Point, int) {
    dragging_ = false;
    invalidate();
}

}

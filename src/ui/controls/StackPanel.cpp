#include "ui/controls/StackPanel.h"

namespace citron::ui {

StackPanel::StackPanel(Orientation orientation, float gap) : orientation_(orientation), gap_(gap) {}

void StackPanel::setPadding(float left, float top, float right, float bottom) {
    padLeft_ = left;
    padTop_ = top;
    padRight_ = right;
    padBottom_ = bottom;
}

Size StackPanel::measure(const Size& available) {
    const Size inner{std::max(0.0f, available.w - padLeft_ - padRight_), std::max(0.0f, available.h - padTop_ - padBottom_)};
    float main = 0.0f;
    float cross = 0.0f;
    int count = 0;
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }
        const Size s = child->measure(inner);
        if (orientation_ == Orientation::Vertical) {
            main += s.h;
            cross = std::max(cross, s.w);
        } else {
            main += s.w;
            cross = std::max(cross, s.h);
        }
        ++count;
    }
    if (count > 1) {
        main += gap_ * static_cast<float>(count - 1);
    }
    if (orientation_ == Orientation::Vertical) {
        return {cross + padLeft_ + padRight_, main + padTop_ + padBottom_};
    }
    return {main + padLeft_ + padRight_, cross + padTop_ + padBottom_};
}

void StackPanel::arrange(const Rect& bounds) {
    bounds_ = bounds;
    const Rect inner = bounds.inset(padLeft_, padTop_, padRight_, padBottom_);
    std::vector<Element*> visible;
    for (const auto& child : children()) {
        if (child->visible()) {
            visible.push_back(child.get());
        }
    }
    float cursor = orientation_ == Orientation::Vertical ? inner.y : inner.x;
    for (size_t i = 0; i < visible.size(); ++i) {
        Element* child = visible[i];
        Size s = child->measure({inner.w, inner.h});
        const bool last = i + 1 == visible.size();
        if (orientation_ == Orientation::Vertical) {
            float h = s.h;
            if (last && stretchLast_) {
                h = std::max(h, inner.bottom() - cursor);
            }
            float w = fill_ ? inner.w : std::min(s.w, inner.w);
            float x = inner.x;
            if (!fill_) {
                if (crossAlign_ == Align::Center) {
                    x = inner.x + (inner.w - w) / 2.0f;
                } else if (crossAlign_ == Align::End) {
                    x = inner.right() - w;
                }
            }
            child->arrange({x, cursor, w, h});
            cursor += h + gap_;
        } else {
            float w = s.w;
            if (last && stretchLast_) {
                w = std::max(w, inner.right() - cursor);
            }
            float h = fill_ ? inner.h : std::min(s.h, inner.h);
            float y = inner.y;
            if (!fill_) {
                if (crossAlign_ == Align::Center) {
                    y = inner.y + (inner.h - h) / 2.0f;
                } else if (crossAlign_ == Align::End) {
                    y = inner.bottom() - h;
                }
            }
            child->arrange({cursor, y, w, h});
            cursor += w + gap_;
        }
    }
}

}

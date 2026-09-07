#include "ui/controls/Tabs.h"

namespace citron::ui {

Tabs::Tabs(TabsKind kind, std::vector<std::wstring> labels, std::function<void(int)> onSelect) : kind_(kind), labels_(std::move(labels)), onSelect_(std::move(onSelect)) {}

void Tabs::setSelected(int index) {
    if (index != selected_ && index >= 0 && index < static_cast<int>(labels_.size())) {
        selected_ = index;
        focusIndex_ = index;
        invalidate();
    }
}

void Tabs::setLabels(std::vector<std::wstring> labels) {
    labels_ = std::move(labels);
    invalidate();
}

TextStyle Tabs::style() const {
    TextStyle s;
    s.size = kind_ == TabsKind::Navigation ? 14.0f : 13.0f;
    s.weight = 700;
    return s;
}

Size Tabs::measure(const Size&) {
    const float height = kind_ == TabsKind::Navigation ? 52.0f : 34.0f;
    const float pad = kind_ == TabsKind::Navigation ? 16.0f : 0.0f;
    const float gap = kind_ == TabsKind::Navigation ? 2.0f : 18.0f;
    float width = 0.0f;
    for (size_t i = 0; i < labels_.size(); ++i) {
        const float w = host_ != nullptr ? host_->measureText(labels_[i], style()).w : 0.0f;
        width += w + pad * 2.0f;
        if (i + 1 < labels_.size()) {
            width += gap;
        }
    }
    return {width, height};
}

void Tabs::arrange(const Rect& bounds) {
    bounds_ = bounds;
    rects_.clear();
    const float pad = kind_ == TabsKind::Navigation ? 16.0f : 0.0f;
    const float gap = kind_ == TabsKind::Navigation ? 2.0f : 18.0f;
    float x = bounds.x;
    for (const auto& label : labels_) {
        const float w = (host_ != nullptr ? host_->measureText(label, style()).w : 0.0f) + pad * 2.0f;
        rects_.push_back({x, bounds.y, w, bounds.h});
        x += w + gap;
    }
}

int Tabs::indexAt(Point point) const {
    for (size_t i = 0; i < rects_.size(); ++i) {
        if (rects_[i].contains(point)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Tabs::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    const float pad = kind_ == TabsKind::Navigation ? 16.0f : 0.0f;
    for (size_t i = 0; i < rects_.size() && i < labels_.size(); ++i) {
        const Rect& r = rects_[i];
        const bool active = static_cast<int>(i) == selected_;
        const bool hover = static_cast<int>(i) == hover_;
        if (hover && !active && kind_ == TabsKind::Navigation) {
            ctx.r.fillRect(r, t.dark ? Color::white(0.03f) : Color::black(0.03f));
        }
        const Color color = active || hover ? t.textHi : t.textDim;
        ctx.r.drawText(labels_[i], style(), r, color, {Align::Center, Align::Center, false, false});
        if (active) {
            ctx.r.fillRect({r.x + pad, r.bottom() - 1.0f, r.w - pad * 2.0f, 2.0f}, t.accent);
        }
        if (focused() && host_->keyboardFocusVisible() && static_cast<int>(i) == focusIndex_) {
            ctx.r.strokeRect(r.inset(2.0f), t.textHi, 2.0f, 4.0f);
        }
    }
}

Element* Tabs::hitTest(Point point) {
    if (!visible() || !enabled()) {
        return nullptr;
    }
    return indexAt(point) >= 0 ? this : nullptr;
}

void Tabs::onMouseMove(Point point) {
    const int index = indexAt(point);
    if (index != hover_) {
        hover_ = index;
        invalidate();
    }
}

void Tabs::onMouseLeave() {
    if (hover_ != -1) {
        hover_ = -1;
        invalidate();
    }
}

void Tabs::onMouseUp(Point point, int button) {
    if (button != 0) {
        return;
    }
    const int index = indexAt(point);
    if (index >= 0 && onSelect_) {
        focusIndex_ = index;
        onSelect_(index);
    }
}

bool Tabs::onKeyDown(unsigned vk, bool, bool) {
    if (vk == VK_LEFT && focusIndex_ > 0) {
        --focusIndex_;
        invalidate();
        return true;
    }
    if (vk == VK_RIGHT && focusIndex_ + 1 < static_cast<int>(labels_.size())) {
        ++focusIndex_;
        invalidate();
        return true;
    }
    if ((vk == VK_RETURN || vk == VK_SPACE) && onSelect_) {
        onSelect_(focusIndex_);
        return true;
    }
    return false;
}

}

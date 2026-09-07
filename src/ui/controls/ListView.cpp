#include "ui/controls/ListView.h"

namespace citron::ui {

void ListRow::setSelected(bool selected) {
    if (selected_ != selected) {
        selected_ = selected;
        invalidate();
    }
}

Size ListRow::measure(const Size& available) {
    return {available.w, height_};
}

void ListRow::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    Color bg = Color::transparent();
    if (selected_) {
        bg = t.selectedTint;
    } else if (hovered() && enabled()) {
        bg = t.hover;
    }
    if (dimmed_) {
        ctx.r.pushOpacity(bounds_, 0.45f);
    }
    ctx.r.fillRect(bounds_, bg, 6.0f);
    renderContent(ctx);
    Element::render(ctx);
    if (dimmed_) {
        ctx.r.popOpacity();
    }
    if (focused() && host_->keyboardFocusVisible()) {
        ctx.r.strokeRect(bounds_, t.textHi, 2.0f, 6.0f);
    }
}

void ListRow::onMouseUp(Point point, int button) {
    if (button == 0 && selectable_ && bounds_.contains(point) && onActivate_) {
        onActivate_();
    }
}

bool ListRow::onKeyDown(unsigned vk, bool, bool) {
    if ((vk == VK_RETURN || vk == VK_SPACE) && selectable_ && onActivate_) {
        onActivate_();
        return true;
    }
    return false;
}

ListView::ListView(float gap) : gap_(gap) {}

void ListView::setPadding(float left, float top, float right, float bottom) {
    padLeft_ = left;
    padTop_ = top;
    padRight_ = right;
    padBottom_ = bottom;
}

Size ListView::measure(const Size& available) {
    const float width = std::max(0.0f, available.w - padLeft_ - padRight_);
    float height = padTop_ + padBottom_;
    int count = 0;
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }
        height += child->measure({width, available.h}).h;
        ++count;
    }
    if (count > 1) {
        height += gap_ * static_cast<float>(count - 1);
    }
    return {available.w, height};
}

void ListView::arrange(const Rect& bounds) {
    bounds_ = bounds;
    const Rect inner = bounds.inset(padLeft_, padTop_, padRight_, padBottom_);
    float y = inner.y;
    for (const auto& child : children()) {
        if (!child->visible()) {
            continue;
        }
        const float h = child->measure({inner.w, inner.h}).h;
        child->arrange({inner.x, y, inner.w, h});
        y += h + gap_;
    }
}

bool ListView::onKeyDown(unsigned vk, bool, bool) {
    if (vk != VK_UP && vk != VK_DOWN) {
        return false;
    }
    std::vector<Element*> rows;
    for (const auto& child : children()) {
        if (child->visible() && child->focusable()) {
            rows.push_back(child.get());
        }
    }
    if (rows.empty() || host_ == nullptr) {
        return false;
    }
    Element* current = host_->focusedElement();
    size_t index = 0;
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i] == current) {
            index = i;
            break;
        }
    }
    if (vk == VK_DOWN && index + 1 < rows.size()) {
        host_->requestFocus(rows[index + 1]);
    } else if (vk == VK_UP && index > 0) {
        host_->requestFocus(rows[index - 1]);
    }
    return true;
}

}

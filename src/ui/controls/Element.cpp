#include "ui/controls/Element.h"

namespace citron::ui {

Element::~Element() {
    if (host_ != nullptr) {
        host_->elementDestroyed(this);
    }
}

Size Element::measure(const Size& available) {
    Size desired;
    for (const auto& child : children_) {
        if (!child->visible()) {
            continue;
        }
        const Size s = child->measure(available);
        desired.w = std::max(desired.w, s.w);
        desired.h = std::max(desired.h, s.h);
    }
    return desired;
}

void Element::arrange(const Rect& bounds) {
    bounds_ = bounds;
    for (const auto& child : children_) {
        if (child->visible()) {
            child->arrange(bounds);
        }
    }
}

void Element::render(RenderContext& ctx) {
    for (const auto& child : children_) {
        if (child->visible()) {
            child->render(ctx);
        }
    }
}

Element* Element::hitTest(Point point) {
    if (!visible_ || !bounds_.contains(point)) {
        return nullptr;
    }
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if (Element* hit = (*it)->hitTest(point)) {
            return hit;
        }
    }
    return interactive() && enabled_ ? this : nullptr;
}

Element* Element::addChild(std::unique_ptr<Element> child) {
    child->parent_ = this;
    child->setHost(host_);
    children_.push_back(std::move(child));
    return children_.back().get();
}

void Element::clearChildren() {
    children_.clear();
}

void Element::setHost(Host* host) {
    host_ = host;
    for (const auto& child : children_) {
        child->setHost(host);
    }
}

void Element::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        invalidate();
    }
}

void Element::setEnabled(bool enabled) {
    if (enabled_ != enabled) {
        enabled_ = enabled;
        invalidate();
    }
}

bool Element::focused() const {
    return host_ != nullptr && host_->focusedElement() == this;
}

void Element::setHovered(bool hovered) {
    if (hovered_ != hovered) {
        hovered_ = hovered;
        if (hovered) {
            onMouseEnter();
        } else {
            onMouseLeave();
        }
        invalidate();
    }
}

void Element::setPressed(bool pressed) {
    if (pressed_ != pressed) {
        pressed_ = pressed;
        invalidate();
    }
}

void Element::invalidate() {
    if (host_ != nullptr) {
        host_->invalidate();
    }
}

void Element::collectFocusable(std::vector<Element*>& out) {
    if (!visible_) {
        return;
    }
    if (focusable() && enabled_) {
        out.push_back(this);
    }
    for (const auto& child : children_) {
        child->collectFocusable(out);
    }
}

}

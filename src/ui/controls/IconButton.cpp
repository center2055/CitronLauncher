#include "ui/controls/IconButton.h"

#include <cmath>

namespace citron::ui {

IconButton::IconButton(Icon icon, IconButtonKind kind, std::function<void()> onClick) : icon_(icon), kind_(kind), onClick_(std::move(onClick)) {
    if (kind == IconButtonKind::Plain) {
        size_ = {24.0f, 24.0f};
        iconSize_ = 15.0f;
        stroke_ = 2.0f;
    }
}

void IconButton::setIcon(Icon icon) {
    if (icon_ != icon) {
        icon_ = icon;
        invalidate();
    }
}

void IconButton::setSpinning(bool spinning) {
    if (spinning_ != spinning) {
        spinning_ = spinning;
        invalidate();
        if (spinning && host_ != nullptr) {
            host_->requestFrame();
        }
    }
}

void IconButton::click() {
    if (enabled() && onClick_) {
        onClick_();
    }
}

Size IconButton::measure(const Size&) {
    return size_;
}

void IconButton::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    const bool hover = hovered() && enabled();
    Color bg = Color::transparent();
    Color fg = t.textMute;
    if (kind_ == IconButtonKind::CaptionClose) {
        if (hover) {
            bg = t.closeHover;
            fg = Color::rgb(0xffffff);
        }
    } else if (kind_ == IconButtonKind::Caption) {
        if (hover) {
            bg = t.sel;
            fg = t.textHi;
        }
    } else if (hover) {
        fg = t.textHi;
    }
    if (kind_ != IconButtonKind::Plain) {
        ctx.r.fillRect(bounds_, bg, 6.0f);
    }
    const Rect box{bounds_.x + (bounds_.w - iconSize_) / 2.0f, bounds_.y + (bounds_.h - iconSize_) / 2.0f, iconSize_, iconSize_};
    const float angle = spinning_ ? static_cast<float>(std::fmod(ctx.now, 800.0) / 800.0 * 360.0) : 0.0f;
    drawIcon(ctx.r, icon_, box, fg, stroke_, angle);
    if (spinning_) {
        host_->requestFrame();
    }
    if (focused() && host_->keyboardFocusVisible()) {
        ctx.r.strokeRect(bounds_.inset(-2.0f), t.textHi, 2.0f, 8.0f);
    }
}

void IconButton::onMouseUp(Point point, int button) {
    if (button == 0 && bounds_.contains(point)) {
        click();
    }
}

bool IconButton::onKeyDown(unsigned vk, bool, bool) {
    if (vk == VK_SPACE || vk == VK_RETURN) {
        click();
        return true;
    }
    return false;
}

}

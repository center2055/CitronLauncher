#include "ui/controls/Button.h"

#include <cmath>

namespace citron::ui {

Button::Button(std::wstring label, ButtonKind kind, std::function<void()> onClick) : label_(std::move(label)), kind_(kind), onClick_(std::move(onClick)) {
    switch (kind) {
    case ButtonKind::Primary:
        height_ = 46.0f;
        padLeft_ = 20.0f;
        padRight_ = 22.0f;
        fontSize_ = 16.0f;
        break;
    case ButtonKind::Secondary:
        height_ = 46.0f;
        padLeft_ = padRight_ = 16.0f;
        break;
    case ButtonKind::Ghost:
        height_ = 38.0f;
        padLeft_ = padRight_ = 14.0f;
        break;
    case ButtonKind::Row:
    case ButtonKind::RowMuted:
        height_ = 30.0f;
        padLeft_ = padRight_ = 12.0f;
        fontSize_ = 12.0f;
        radius_ = 6.0f;
        break;
    case ButtonKind::Segment:
        height_ = 30.0f;
        padLeft_ = padRight_ = 14.0f;
        fontSize_ = 13.0f;
        weight_ = 700;
        radius_ = 6.0f;
        break;
    case ButtonKind::Link:
        height_ = 20.0f;
        padLeft_ = padRight_ = 0.0f;
        fontSize_ = 13.0f;
        weight_ = 600;
        radius_ = 3.0f;
        break;
    }
}

void Button::setLabel(std::wstring label) {
    if (label_ != label) {
        label_ = std::move(label);
        invalidate();
    }
}

void Button::setKind(ButtonKind kind) {
    kind_ = kind;
    invalidate();
}

void Button::setBusy(bool busy) {
    if (busy_ != busy) {
        busy_ = busy;
        invalidate();
        if (busy && host_ != nullptr) {
            host_->requestFrame();
        }
    }
}

void Button::setArmed(bool armed) {
    if (armed_ != armed) {
        armed_ = armed;
        invalidate();
    }
}

void Button::setSelected(bool selected) {
    if (selected_ != selected) {
        selected_ = selected;
        invalidate();
    }
}

void Button::click() {
    if (enabled() && !busy_ && onClick_) {
        onClick_();
    }
}

TextStyle Button::style() const {
    TextStyle s;
    s.size = fontSize_;
    s.weight = weight_;
    s.letterSpacing = kind_ == ButtonKind::Primary ? fontSize_ * 0.01f : 0.0f;
    return s;
}

Size Button::measure(const Size&) {
    float width = padLeft_ + padRight_;
    if (host_ != nullptr && !label_.empty()) {
        width += host_->measureText(label_, style()).w;
    }
    if (trailingIcon_ || busy_) {
        width += (kind_ == ButtonKind::Primary ? 14.0f : 8.0f) + std::max(iconSize_, busy_ ? 14.0f : 0.0f);
    }
    if (leadingIcon_) {
        width += 8.0f + iconSize_;
    }
    return {std::ceil(width), height_};
}

void Button::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    const bool hover = hovered() && enabled() && !busy_;
    const bool press = pressed() && hover;
    Color bg = Color::transparent();
    Color border = Color::transparent();
    Color fg = t.textHi;
    float yOffset = 0.0f;

    switch (kind_) {
    case ButtonKind::Primary:
        bg = !enabled() ? t.sel : press ? t.accentPressed : hover ? t.accentHover : t.accent;
        fg = enabled() ? t.accentInk : t.textDim;
        if (press) {
            yOffset = 1.0f;
        }
        break;
    case ButtonKind::Secondary:
        border = !enabled() ? t.hairline : (hover || press) ? mix(t.border2, t.border3, 0.9f) : t.border2;
        bg = press ? Color::white(0.01f) : hover ? t.inset : Color::transparent();
        fg = !enabled() ? t.textFaint : hover ? t.textHi : t.textBody;
        break;
    case ButtonKind::Ghost:
        fg = hover ? t.textHi : t.textMute;
        break;
    case ButtonKind::Row:
        border = hover ? t.border3 : t.border2;
        fg = t.textHi;
        break;
    case ButtonKind::RowMuted:
        if (armed_) {
            border = t.danger;
            bg = t.dangerTint;
            fg = t.danger;
        } else if (hover && dangerHover_) {
            border = t.danger;
            fg = t.danger;
        } else {
            border = t.border2;
            fg = t.textMute;
        }
        break;
    case ButtonKind::Segment:
        bg = selected_ ? t.accent : Color::transparent();
        fg = selected_ ? t.accentInk : hover ? t.textHi : t.textMute;
        break;
    case ButtonKind::Link:
        fg = hover ? mix(t.accentText, t.textHi, 0.35f) : t.accentText;
        break;
    }
    if (!enabled() && kind_ != ButtonKind::Primary && kind_ != ButtonKind::Secondary) {
        fg = t.textFaint;
    }

    Rect box = bounds_;
    box.y += yOffset;
    if (busy_ && kind_ == ButtonKind::Primary) {
        ctx.r.pushOpacity(box, 0.85f);
    }
    ctx.r.fillRect(box, bg, radius_);
    if (border.a > 0.0f) {
        ctx.r.strokeRect(box, border, 1.0f, radius_);
    }

    float x = box.x + padLeft_;
    const float iconGap = kind_ == ButtonKind::Primary ? 14.0f : 8.0f;
    if (busy_) {
        const float size = 14.0f;
        const float angle = static_cast<float>(std::fmod(ctx.now, 800.0) / 800.0 * 360.0);
        drawIcon(ctx.r, Icon::Spinner, {x, box.y + (box.h - size) / 2.0f, size, size}, fg, kind_ == ButtonKind::Primary ? 3.0f : 2.4f, angle);
        x += size + iconGap;
        host_->requestFrame();
    } else if (leadingIcon_) {
        drawIcon(ctx.r, *leadingIcon_, {x, box.y + (box.h - iconSize_) / 2.0f, iconSize_, iconSize_}, fg, 2.2f);
        x += iconSize_ + iconGap;
    }
    const float labelWidth = host_ != nullptr ? host_->measureText(label_, style()).w : 0.0f;
    ctx.r.drawText(label_, style(), {x, box.y, std::max(0.0f, box.right() - padRight_ - x), box.h}, fg, {Align::Start, Align::Center, true, false});
    if (trailingIcon_ && !busy_) {
        const float ix = x + labelWidth + iconGap;
        drawIcon(ctx.r, *trailingIcon_, {ix, box.y + (box.h - iconSize_) / 2.0f, iconSize_, iconSize_}, fg, 2.6f);
    }
    if (busy_ && kind_ == ButtonKind::Primary) {
        ctx.r.popOpacity();
    }
    if (focused() && host_->keyboardFocusVisible()) {
        ctx.r.strokeRect(bounds_.inset(-2.0f), t.textHi, 2.0f, radius_ + 2.0f);
    }
}

void Button::onMouseUp(Point point, int button) {
    if (button == 0 && bounds_.contains(point)) {
        click();
    }
}

Cursor Button::cursor() const {
    return kind_ == ButtonKind::Link ? Cursor::Hand : Cursor::Arrow;
}

bool Button::onKeyDown(unsigned vk, bool, bool) {
    if (vk == VK_SPACE || vk == VK_RETURN) {
        click();
        return true;
    }
    return false;
}

}

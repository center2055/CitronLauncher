#include "ui/controls/Text.h"

namespace citron::ui {

Text::Text(std::wstring text, TextStyle style, TextRole role) : text_(std::move(text)), style_(style), role_(role) {}

void Text::setText(std::wstring text) {
    if (text_ != text) {
        text_ = std::move(text);
        invalidate();
    }
}

void Text::setStyle(const TextStyle& style) {
    style_ = style;
    invalidate();
}

void Text::setRole(TextRole role) {
    role_ = role;
    invalidate();
}

void Text::setColor(Color color) {
    role_ = TextRole::Custom;
    custom_ = color;
    invalidate();
}

void Text::setAlign(Align horizontal, Align vertical) {
    horizontal_ = horizontal;
    vertical_ = vertical;
}

void Text::setWrap(bool wrap) {
    wrap_ = wrap;
}

Color Text::roleColor(const Theme& theme, TextRole role) {
    switch (role) {
    case TextRole::Hi: return theme.textHi;
    case TextRole::Body: return theme.textBody;
    case TextRole::Mute: return theme.textMute;
    case TextRole::Dim: return theme.textDim;
    case TextRole::Faint: return theme.textFaint;
    case TextRole::Label: return theme.textLabel;
    case TextRole::Accent: return theme.accentText;
    case TextRole::Danger: return theme.danger;
    case TextRole::Warn: return theme.warn;
    case TextRole::Custom: return theme.textHi;
    }
    return theme.textHi;
}

Size Text::measure(const Size& available) {
    if (host_ == nullptr || text_.empty()) {
        return {minWidth_, 0.0f};
    }
    const float limit = std::min(maxWidth_, wrap_ ? available.w : 1e6f);
    Size s = host_->measureText(text_, style_, limit, wrap_);
    s.w = std::max(minWidth_, std::min(s.w, limit));
    return s;
}

void Text::render(RenderContext& ctx) {
    if (text_.empty()) {
        return;
    }
    TextOptions options;
    options.horizontal = horizontal_;
    options.vertical = vertical_;
    options.wrap = wrap_;
    options.ellipsis = ellipsis_;
    const Color color = role_ == TextRole::Custom ? custom_ : roleColor(ctx.theme, role_);
    ctx.r.drawText(text_, style_, bounds_, color, options);
}

}

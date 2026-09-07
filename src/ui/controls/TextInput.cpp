#include "ui/controls/TextInput.h"

#include "platform/windows/Shell.h"

#include <cmath>

namespace citron::ui {

TextInput::TextInput(std::wstring placeholder, std::function<void(const std::wstring&)> onChange) : placeholder_(std::move(placeholder)), onChange_(std::move(onChange)) {
    style_.size = 14.0f;
    style_.weight = 400;
}

void TextInput::setText(std::wstring text) {
    text_ = std::move(text);
    caret_ = std::min(caret_, text_.size());
    anchor_ = caret_;
    invalidate();
}

Size TextInput::measure(const Size& available) {
    return {available.w, 22.0f};
}

float TextInput::caretX(size_t index) {
    if (host_ == nullptr || index == 0) {
        return 0.0f;
    }
    return host_->measureText(std::wstring_view(text_).substr(0, index), style_).w;
}

size_t TextInput::positionFromPoint(Point point) {
    if (host_ == nullptr) {
        return 0;
    }
    const float x = point.x - bounds_.x + scroll_;
    auto layout = host_->renderer().createLayout(text_, style_, 1e6f, bounds_.h, false);
    if (!layout) {
        return text_.size();
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metrics{};
    layout->HitTestPoint(x, bounds_.h / 2.0f, &trailing, &inside, &metrics);
    size_t index = metrics.textPosition + (trailing ? metrics.length : 0);
    return std::min(index, text_.size());
}

void TextInput::ensureCaretVisible() {
    const float x = caretX(caret_);
    if (x - scroll_ > bounds_.w - 2.0f) {
        scroll_ = x - bounds_.w + 2.0f;
    } else if (x - scroll_ < 0.0f) {
        scroll_ = x;
    }
    scroll_ = std::max(0.0f, scroll_);
}

void TextInput::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    ctx.r.pushClip(bounds_);
    if (text_.empty()) {
        ctx.r.drawText(placeholder_, style_, bounds_, t.textDim, {Align::Start, Align::Center, true, false});
    } else {
        if (hasSelection() && focused()) {
            const float x0 = caretX(selectionStart()) - scroll_;
            const float x1 = caretX(selectionEnd()) - scroll_;
            ctx.r.fillRect({bounds_.x + x0, bounds_.y + 2.0f, x1 - x0, bounds_.h - 4.0f}, t.accent.withAlpha(0.35f), 2.0f);
        }
        const Rect textRect{bounds_.x - scroll_, bounds_.y, bounds_.w + scroll_ + 4.0f, bounds_.h};
        ctx.r.drawText(text_, style_, textRect, t.textHi, {Align::Start, Align::Center, false, false});
    }
    if (focused()) {
        const double phase = std::fmod(ctx.now - blinkStart_, 1060.0);
        if (phase < 530.0) {
            const float x = bounds_.x + caretX(caret_) - scroll_;
            ctx.r.fillRect({std::round(x), bounds_.y + 3.0f, 1.0f, bounds_.h - 6.0f}, t.textHi);
        }
        const double untilToggle = phase < 530.0 ? 530.0 - phase : 1060.0 - phase;
        host_->requestFrameIn(static_cast<unsigned>(untilToggle) + 1);
    }
    ctx.r.popClip();
}

void TextInput::onMouseDown(Point point, int button) {
    if (button != 0) {
        return;
    }
    if (host_ != nullptr) {
        host_->requestFocus(this);
    }
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    moveCaret(positionFromPoint(point), shift);
    dragging_ = true;
}

void TextInput::onMouseMove(Point point) {
    if (dragging_) {
        moveCaret(positionFromPoint(point), true);
    }
}

void TextInput::onMouseUp(Point, int) {
    dragging_ = false;
}

void TextInput::onDoubleClick(Point) {
    anchor_ = 0;
    caret_ = text_.size();
    invalidate();
}

void TextInput::moveCaret(size_t index, bool extend) {
    caret_ = std::min(index, text_.size());
    if (!extend) {
        anchor_ = caret_;
    }
    blinkStart_ = host_ != nullptr ? host_->now() : 0.0;
    ensureCaretVisible();
    invalidate();
}

void TextInput::deleteSelection() {
    if (!hasSelection()) {
        return;
    }
    const size_t start = selectionStart();
    text_.erase(start, selectionEnd() - start);
    caret_ = anchor_ = start;
}

void TextInput::replaceSelection(std::wstring_view insert) {
    deleteSelection();
    text_.insert(caret_, insert);
    caret_ += insert.size();
    anchor_ = caret_;
    notify();
}

void TextInput::notify() {
    blinkStart_ = host_ != nullptr ? host_->now() : 0.0;
    ensureCaretVisible();
    invalidate();
    if (onChange_) {
        onChange_(text_);
    }
}

bool TextInput::onKeyDown(unsigned vk, bool ctrl, bool shift) {
    switch (vk) {
    case VK_LEFT:
        if (hasSelection() && !shift) {
            moveCaret(selectionStart(), false);
        } else if (caret_ > 0) {
            moveCaret(caret_ - 1, shift);
        }
        return true;
    case VK_RIGHT:
        if (hasSelection() && !shift) {
            moveCaret(selectionEnd(), false);
        } else if (caret_ < text_.size()) {
            moveCaret(caret_ + 1, shift);
        }
        return true;
    case VK_HOME:
        moveCaret(0, shift);
        return true;
    case VK_END:
        moveCaret(text_.size(), shift);
        return true;
    case VK_BACK:
        if (hasSelection()) {
            deleteSelection();
            notify();
        } else if (caret_ > 0) {
            text_.erase(caret_ - 1, 1);
            --caret_;
            anchor_ = caret_;
            notify();
        }
        return true;
    case VK_DELETE:
        if (hasSelection()) {
            deleteSelection();
            notify();
        } else if (caret_ < text_.size()) {
            text_.erase(caret_, 1);
            notify();
        }
        return true;
    case VK_ESCAPE:
        if (!text_.empty()) {
            text_.clear();
            caret_ = anchor_ = 0;
            notify();
            return true;
        }
        return false;
    default:
        break;
    }
    if (ctrl) {
        switch (vk) {
        case 'A':
            anchor_ = 0;
            caret_ = text_.size();
            invalidate();
            return true;
        case 'C':
        case 'X':
            if (hasSelection()) {
                platform::copyToClipboard(nullptr, std::wstring_view(text_).substr(selectionStart(), selectionEnd() - selectionStart()));
                if (vk == 'X') {
                    deleteSelection();
                    notify();
                }
            }
            return true;
        case 'V':
            if (auto pasted = platform::readClipboard(nullptr)) {
                std::wstring clean;
                for (const wchar_t c : *pasted) {
                    if (c >= 0x20) {
                        clean.push_back(c);
                    }
                }
                replaceSelection(clean);
            }
            return true;
        default:
            break;
        }
    }
    return false;
}

void TextInput::onChar(wchar_t c) {
    if (c < 0x20 || c == 0x7F) {
        return;
    }
    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        return;
    }
    replaceSelection(std::wstring_view(&c, 1));
}

void TextInput::onFocusChanged(bool focused) {
    if (focused) {
        blinkStart_ = host_ != nullptr ? host_->now() : 0.0;
    } else {
        dragging_ = false;
    }
    invalidate();
}

}

#include "ui/controls/Dialog.h"

namespace citron::ui {

namespace {
const TextStyle kTitleStyle{Font::Title, 16.0f, 600, 0.0f};
const TextStyle kBodyStyle{Font::Body, 13.0f, 400, 0.0f};
constexpr float kPanelWidth = 440.0f;
constexpr float kPad = 22.0f;
}

Dialog::Dialog() {
    setVisible(false);
}

void Dialog::show(std::wstring title, std::vector<std::wstring> paragraphs, std::wstring confirmLabel, std::wstring cancelLabel, bool destructive,
                  std::function<void()> onConfirm, std::function<void()> onCancel) {
    clearChildren();
    title_ = std::move(title);
    paragraphs_ = std::move(paragraphs);
    destructive_ = destructive;
    onCancel_ = std::move(onCancel);
    cancelButton_ = nullptr;
    if (!cancelLabel.empty()) {
        auto cancelBtn = std::make_unique<Button>(std::move(cancelLabel), ButtonKind::Secondary, [this] { cancel(); });
        cancelBtn->setHeight(36.0f);
        cancelBtn->setPadding(14.0f, 14.0f);
        cancelBtn->setFontSize(13.0f);
        cancelButton_ = add(std::move(cancelBtn));
    }
    auto confirmBtn = std::make_unique<Button>(std::move(confirmLabel), destructive ? ButtonKind::RowMuted : ButtonKind::Primary, [this, onConfirm = std::move(onConfirm)] {
        hide();
        if (onConfirm) {
            onConfirm();
        }
    });
    confirmBtn->setHeight(36.0f);
    confirmBtn->setPadding(16.0f, 16.0f);
    confirmBtn->setFontSize(13.0f);
    confirmBtn->setRadius(8.0f);
    if (destructive) {
        confirmBtn->setArmed(true);
    }
    confirm_ = add(std::move(confirmBtn));
    setVisible(true);
    if (host_ != nullptr) {
        host_->requestFocus(cancelButton_ != nullptr ? cancelButton_ : confirm_);
    }
    invalidate();
}

void Dialog::hide() {
    setVisible(false);
    invalidate();
}

void Dialog::cancel() {
    hide();
    if (onCancel_) {
        onCancel_();
    }
}

Size Dialog::measure(const Size& available) {
    return available;
}

void Dialog::arrange(const Rect& bounds) {
    bounds_ = bounds;
    const float width = std::min(kPanelWidth, bounds.w - 40.0f);
    const float textWidth = width - kPad * 2.0f;
    float height = kPad + 24.0f + 12.0f;
    paragraphHeights_.clear();
    for (const auto& p : paragraphs_) {
        const float h = host_ != nullptr ? host_->measureText(p, kBodyStyle, textWidth, true).h : 20.0f;
        paragraphHeights_.push_back(h);
        height += h + 10.0f;
    }
    height += 10.0f + 36.0f + kPad;
    panel_ = {bounds.x + (bounds.w - width) / 2.0f, bounds.y + (bounds.h - height) / 2.0f, width, height};
    if (confirm_ != nullptr) {
        const Size cs = confirm_->measure({});
        const float y = panel_.bottom() - kPad - 36.0f;
        confirm_->arrange({panel_.right() - kPad - cs.w, y, cs.w, 36.0f});
        if (cancelButton_ != nullptr) {
            const Size xs = cancelButton_->measure({});
            cancelButton_->arrange({panel_.right() - kPad - cs.w - 10.0f - xs.w, y, xs.w, 36.0f});
        }
    }
}

void Dialog::render(RenderContext& ctx) {
    const Theme& t = ctx.theme;
    ctx.r.fillRect(bounds_, t.scrim);
    ctx.r.fillRect(panel_, t.raised, 12.0f);
    ctx.r.strokeRect(panel_, t.border2, 1.0f, 12.0f);
    float y = panel_.y + kPad;
    const float textX = panel_.x + kPad;
    const float textWidth = panel_.w - kPad * 2.0f;
    ctx.r.drawText(title_, kTitleStyle, {textX, y, textWidth, 24.0f}, t.textHi, {Align::Start, Align::Center, true, false});
    y += 24.0f + 12.0f;
    for (size_t i = 0; i < paragraphs_.size(); ++i) {
        const float h = i < paragraphHeights_.size() ? paragraphHeights_[i] : 20.0f;
        ctx.r.drawText(paragraphs_[i], kBodyStyle, {textX, y, textWidth, h}, t.textMute, {Align::Start, Align::Start, false, true});
        y += h + 10.0f;
    }
    Element::render(ctx);
}

Element* Dialog::hitTest(Point point) {
    if (!visible()) {
        return nullptr;
    }
    for (auto it = children().rbegin(); it != children().rend(); ++it) {
        if (Element* hit = (*it)->hitTest(point)) {
            return hit;
        }
    }
    return this;
}

bool Dialog::onKeyDown(unsigned vk, bool, bool) {
    if (vk == VK_ESCAPE) {
        cancel();
        return true;
    }
    return false;
}

void Dialog::onMouseUp(Point point, int button) {
    if (button == 0 && !panel_.contains(point)) {
        cancel();
    }
}

}

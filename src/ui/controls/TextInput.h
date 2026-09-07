#pragma once

#include "ui/controls/Element.h"

#include <functional>
#include <string>

namespace citron::ui {

class TextInput : public Element {
public:
    TextInput(std::wstring placeholder, std::function<void(const std::wstring&)> onChange);

    const std::wstring& text() const { return text_; }
    void setText(std::wstring text);
    void setPlaceholder(std::wstring placeholder) { placeholder_ = std::move(placeholder); }
    void setFontSize(float size) { style_.size = size; }

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return true; }
    Cursor cursor() const override { return Cursor::IBeam; }
    void onMouseDown(Point point, int button) override;
    void onMouseMove(Point point) override;
    void onMouseUp(Point point, int button) override;
    void onDoubleClick(Point point) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;
    void onChar(wchar_t c) override;
    void onFocusChanged(bool focused) override;

private:
    size_t positionFromPoint(Point point);
    float caretX(size_t index);
    void deleteSelection();
    void replaceSelection(std::wstring_view insert);
    void notify();
    bool hasSelection() const { return anchor_ != caret_; }
    size_t selectionStart() const { return std::min(anchor_, caret_); }
    size_t selectionEnd() const { return std::max(anchor_, caret_); }
    void moveCaret(size_t index, bool extend);
    void ensureCaretVisible();

    std::wstring text_;
    std::wstring placeholder_;
    std::function<void(const std::wstring&)> onChange_;
    TextStyle style_;
    size_t caret_ = 0;
    size_t anchor_ = 0;
    float scroll_ = 0.0f;
    bool dragging_ = false;
    double blinkStart_ = 0.0;
};

}

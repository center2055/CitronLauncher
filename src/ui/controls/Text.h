#pragma once

#include "ui/controls/Element.h"

#include <string>

namespace citron::ui {

enum class TextRole {
    Hi,
    Body,
    Mute,
    Dim,
    Faint,
    Label,
    Accent,
    Danger,
    Warn,
    Custom,
};

class Text : public Element {
public:
    Text(std::wstring text, TextStyle style, TextRole role = TextRole::Hi);

    void setText(std::wstring text);
    const std::wstring& text() const { return text_; }
    void setStyle(const TextStyle& style);
    void setRole(TextRole role);
    void setColor(Color color);
    void setAlign(Align horizontal, Align vertical = Align::Center);
    void setWrap(bool wrap);
    void setEllipsis(bool ellipsis) { ellipsis_ = ellipsis; }
    void setMaxWidth(float maxWidth) { maxWidth_ = maxWidth; }
    void setMinWidth(float minWidth) { minWidth_ = minWidth; }

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;

    static Color roleColor(const Theme& theme, TextRole role);

private:
    std::wstring text_;
    TextStyle style_;
    TextRole role_;
    Color custom_;
    Align horizontal_ = Align::Start;
    Align vertical_ = Align::Center;
    bool wrap_ = false;
    bool ellipsis_ = true;
    float maxWidth_ = 1e6f;
    float minWidth_ = 0.0f;
};

}

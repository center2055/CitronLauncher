#pragma once

#include "ui/Animation.h"
#include "ui/Icons.h"
#include "ui/controls/Element.h"

#include <functional>
#include <optional>
#include <string>

namespace citron::ui {

enum class ButtonKind {
    Primary,
    Secondary,
    Ghost,
    Row,
    RowMuted,
    Segment,
    Link,
};

class Button : public Element {
public:
    Button(std::wstring label, ButtonKind kind, std::function<void()> onClick);

    void setLabel(std::wstring label);
    const std::wstring& label() const { return label_; }
    void setKind(ButtonKind kind);
    void setHeight(float height) { height_ = height; }
    void setPadding(float left, float right) { padLeft_ = left; padRight_ = right; }
    void setFontSize(float size) { fontSize_ = size; }
    void setWeight(int weight) { weight_ = weight; }
    void setRadius(float radius) { radius_ = radius; }
    void setTrailingIcon(std::optional<Icon> icon, float size) { trailingIcon_ = icon; iconSize_ = size; }
    void setLeadingIcon(std::optional<Icon> icon, float size) { leadingIcon_ = icon; iconSize_ = size; }
    void setBusy(bool busy);
    bool busy() const { return busy_; }
    void setArmed(bool armed);
    bool armed() const { return armed_; }
    void setDangerHover(bool danger) { dangerHover_ = danger; }
    void setSelected(bool selected);
    void setOnClick(std::function<void()> onClick) { onClick_ = std::move(onClick); }
    void click();

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;
    bool interactive() const override { return true; }
    bool focusable() const override { return true; }
    Cursor cursor() const override;
    void onMouseUp(Point point, int button) override;
    bool onKeyDown(unsigned vk, bool ctrl, bool shift) override;

private:
    TextStyle style() const;

    std::wstring label_;
    ButtonKind kind_;
    std::function<void()> onClick_;
    float height_ = 40.0f;
    float padLeft_ = 16.0f;
    float padRight_ = 16.0f;
    float fontSize_ = 14.0f;
    int weight_ = 600;
    float radius_ = 8.0f;
    std::optional<Icon> trailingIcon_;
    std::optional<Icon> leadingIcon_;
    float iconSize_ = 14.0f;
    bool busy_ = false;
    bool armed_ = false;
    bool dangerHover_ = false;
    bool selected_ = false;
};

}

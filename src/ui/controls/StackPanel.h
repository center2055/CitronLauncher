#pragma once

#include "ui/controls/Element.h"

namespace citron::ui {

enum class Orientation {
    Vertical,
    Horizontal,
};

class StackPanel : public Element {
public:
    explicit StackPanel(Orientation orientation = Orientation::Vertical, float gap = 0.0f);

    void setGap(float gap) { gap_ = gap; }
    void setPadding(float left, float top, float right, float bottom);
    void setCrossAlign(Align align) { crossAlign_ = align; }
    void setStretchLast(bool stretch) { stretchLast_ = stretch; }
    void setFill(bool fill) { fill_ = fill; }

    Size measure(const Size& available) override;
    void arrange(const Rect& bounds) override;

private:
    Orientation orientation_;
    float gap_;
    float padLeft_ = 0.0f, padTop_ = 0.0f, padRight_ = 0.0f, padBottom_ = 0.0f;
    Align crossAlign_ = Align::Start;
    bool stretchLast_ = false;
    bool fill_ = false;
};

}

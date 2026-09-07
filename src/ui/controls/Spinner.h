#pragma once

#include "ui/controls/Element.h"

namespace citron::ui {

class Spinner : public Element {
public:
    explicit Spinner(float size = 14.0f, float stroke = 2.4f);

    Size measure(const Size& available) override;
    void render(RenderContext& ctx) override;

private:
    float size_;
    float stroke_;
};

}

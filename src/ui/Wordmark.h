#pragma once

#include "ui/Geometry.h"
#include "ui/Renderer.h"

namespace citron::ui {

class Wordmark {
public:
    void draw(Renderer& renderer, Point origin, float height, Color color);
    static float widthForHeight(float height);

private:
    winrt::com_ptr<ID2D1PathGeometry> geometry_;
    unsigned generation_ = 0;
    ID2D1Factory* factory_ = nullptr;
};

}

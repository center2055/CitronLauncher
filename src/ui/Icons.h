#pragma once

#include "ui/Geometry.h"
#include "ui/Renderer.h"

namespace citron::ui {

enum class Icon {
    Sun,
    Moon,
    Minimize,
    Maximize,
    Restore,
    Close,
    Search,
    Refresh,
    ArrowRight,
    Spinner,
};

void drawIcon(Renderer& renderer, Icon icon, const Rect& box, Color color, float strokeWidth, float rotationDegrees = 0.0f);

}

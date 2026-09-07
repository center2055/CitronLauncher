#include "ui/controls/Spinner.h"

#include "ui/Icons.h"

#include <cmath>

namespace citron::ui {

Spinner::Spinner(float size, float stroke) : size_(size), stroke_(stroke) {}

Size Spinner::measure(const Size&) {
    return {size_, size_};
}

void Spinner::render(RenderContext& ctx) {
    const float angle = static_cast<float>(std::fmod(ctx.now, 800.0) / 800.0 * 360.0);
    drawIcon(ctx.r, Icon::Spinner, bounds_, ctx.theme.textMute, stroke_, angle);
    host_->requestFrame();
}

}

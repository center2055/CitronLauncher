#include "ui/Animation.h"

#include <chrono>
#include <cmath>

namespace citron::ui {

double nowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

Animated::Animated(float value, double durationMs) : from_(value), current_(value), target_(value), durationMs_(durationMs) {}

void Animated::set(float target, bool immediate) {
    if (immediate) {
        jump(target);
        return;
    }
    if (std::fabs(target - target_) < 1e-4f) {
        return;
    }
    from_ = current_;
    target_ = target;
    startMs_ = nowMs();
    active_ = true;
}

void Animated::jump(float value) {
    from_ = value;
    current_ = value;
    target_ = value;
    active_ = false;
}

bool Animated::step(double now) {
    if (!active_) {
        return false;
    }
    double t = (now - startMs_) / durationMs_;
    if (t >= 1.0) {
        current_ = target_;
        active_ = false;
        return false;
    }
    if (t < 0.0) {
        t = 0.0;
    }
    const double eased = 1.0 - std::pow(1.0 - t, 3.0);
    current_ = from_ + (target_ - from_) * static_cast<float>(eased);
    return true;
}

}

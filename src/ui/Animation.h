#pragma once

namespace citron::ui {

double nowMs();

class Animated {
public:
    explicit Animated(float value = 0.0f, double durationMs = 200.0);

    void set(float target, bool immediate = false);
    void jump(float value);
    float value() const { return current_; }
    float target() const { return target_; }
    bool active() const { return active_; }
    bool step(double now);

private:
    float from_ = 0.0f;
    float current_ = 0.0f;
    float target_ = 0.0f;
    double startMs_ = 0.0;
    double durationMs_ = 200.0;
    bool active_ = false;
};

}

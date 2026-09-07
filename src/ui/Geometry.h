#pragma once

#include <algorithm>
#include <cstdint>

namespace citron::ui {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Size {
    float w = 0.0f;
    float h = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float right() const { return x + w; }
    float bottom() const { return y + h; }
    Point center() const { return {x + w / 2.0f, y + h / 2.0f}; }
    bool contains(Point p) const { return p.x >= x && p.y >= y && p.x < right() && p.y < bottom(); }
    bool empty() const { return w <= 0.0f || h <= 0.0f; }

    Rect inset(float all) const { return inset(all, all, all, all); }
    Rect inset(float left, float top, float rightSide, float bottomSide) const {
        return {x + left, y + top, std::max(0.0f, w - left - rightSide), std::max(0.0f, h - top - bottomSide)};
    }
    Rect intersect(const Rect& other) const {
        const float l = std::max(x, other.x);
        const float t = std::max(y, other.y);
        const float r = std::min(right(), other.right());
        const float b = std::min(bottom(), other.bottom());
        return {l, t, std::max(0.0f, r - l), std::max(0.0f, b - t)};
    }
    static Rect ltrb(float l, float t, float r, float b) { return {l, t, r - l, b - t}; }
};

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    static constexpr Color rgb(std::uint32_t hex, float alpha = 1.0f) {
        return {static_cast<float>((hex >> 16) & 0xFF) / 255.0f, static_cast<float>((hex >> 8) & 0xFF) / 255.0f,
                static_cast<float>(hex & 0xFF) / 255.0f, alpha};
    }
    static constexpr Color white(float alpha) { return {1.0f, 1.0f, 1.0f, alpha}; }
    static constexpr Color black(float alpha) { return {0.0f, 0.0f, 0.0f, alpha}; }
    constexpr Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
    static constexpr Color transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    bool operator==(const Color&) const = default;
};

inline Color mix(const Color& a, const Color& b, float t) {
    return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t};
}

}

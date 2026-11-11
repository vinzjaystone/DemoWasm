#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace ashfall {

constexpr float Pi = 3.14159265358979323846f;
constexpr float Sqrt2 = 1.4142135623730950488f;

// Simulation coordinates are in tiles, not pixels. No graphics dependency here.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
    constexpr Vec2 operator+(Vec2 v) const { return {x + v.x, y + v.y}; }
    constexpr Vec2 operator-(Vec2 v) const { return {x - v.x, y - v.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const { return {x / s, y / s}; }
    Vec2& operator+=(Vec2 v) { x += v.x; y += v.y; return *this; }
    Vec2& operator-=(Vec2 v) { x -= v.x; y -= v.y; return *this; }
};

constexpr float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
constexpr float lengthSquared(Vec2 v) { return dot(v, v); }
inline float length(Vec2 v) { return std::sqrt(lengthSquared(v)); }
inline float distance(Vec2 a, Vec2 b) { return length(a - b); }
inline Vec2 normalized(Vec2 v, Vec2 fallback = {1.0f, 0.0f}) {
    const float size = length(v);
    return size > 0.00001f && std::isfinite(size) ? v / size : fallback;
}
inline Vec2 rotated(Vec2 v, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
inline bool finite(Vec2 v) { return std::isfinite(v.x) && std::isfinite(v.y); }
inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

struct TileCoord {
    int x = 0;
    int y = 0;
    constexpr bool operator==(TileCoord v) const { return x == v.x && y == v.y; }
    constexpr bool operator!=(TileCoord v) const { return !(*this == v); }
    constexpr Vec2 center() const {
        return {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};
    }
};
inline TileCoord tileAt(Vec2 p) {
    if (!finite(p)) return {-1, -1};
    // Keep out-of-level inputs safely outside the grid without float-to-int
    // overflow (e.g. when an integration passes a corrupt cursor coordinate).
    constexpr float limit = 1048576.0f;
    return {static_cast<int>(std::floor(std::clamp(p.x, -limit, limit))),
            static_cast<int>(std::floor(std::clamp(p.y, -limit, limit)))};
}

struct Bounds {
    Vec2 center;
    Vec2 halfSize;
    bool contains(Vec2 p) const {
        return std::abs(p.x - center.x) <= halfSize.x && std::abs(p.y - center.y) <= halfSize.y;
    }
};
struct Rgb {
    std::uint8_t r = 255, g = 255, b = 255;
};
inline std::uint32_t hash(std::uint32_t x) {
    x ^= x >> 16u; x *= 0x7feb352du;
    x ^= x >> 15u; x *= 0x846ca68bu;
    return x ^ (x >> 16u);
}

} // namespace ashfall

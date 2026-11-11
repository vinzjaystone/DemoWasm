#include "combat.h"
#include <array>

namespace ashfall {
namespace {
bool pointInSector(Vec2 p, Vec2 origin, Vec2 direction, float range, float cosine) {
    const Vec2 delta = p - origin;
    const float square = lengthSquared(delta);
    return square <= range * range + 0.00001f &&
           (square < 0.000001f || dot(delta, direction) >= std::sqrt(square) * cosine - 0.00001f);
}
bool segmentIntersectsBox(Vec2 start, Vec2 end, Bounds b) {
    const Vec2 delta = end - start;
    float first = 0, last = 1;
    for (int axis = 0; axis < 2; ++axis) {
        const float p = axis == 0 ? start.x : start.y;
        const float d = axis == 0 ? delta.x : delta.y;
        const float center = axis == 0 ? b.center.x : b.center.y;
        const float extent = axis == 0 ? b.halfSize.x : b.halfSize.y;
        if (std::abs(d) < 0.000001f) {
            if (p < center - extent || p > center + extent) return false;
        } else {
            float near = (center - extent - p) / d;
            float far = (center + extent - p) / d;
            if (near > far) std::swap(near, far);
            first = std::max(first, near);
            last = std::min(last, far);
            if (first > last) return false;
        }
    }
    return true;
}
}
bool sectorIntersectsBox(Vec2 origin, Vec2 direction, float range, float halfArc, Bounds b) {
    if (range <= 0 || halfArc <= 0 || !finite(origin) || !finite(direction) ||
        !finite(b.center) || !finite(b.halfSize)) return false;
    direction = normalized(direction);
    halfArc = std::min(halfArc, Pi);
    const float cosine = std::cos(halfArc);
    const Vec2 min = b.center - b.halfSize, max = b.center + b.halfSize;
    const Vec2 closest{std::clamp(origin.x, min.x, max.x), std::clamp(origin.y, min.y, max.y)};
    if (lengthSquared(closest - origin) > range * range) return false;
    if (b.contains(origin)) return true;
    const std::array<Vec2, 4> corners = {{{min.x,min.y}, {max.x,min.y}, {max.x,max.y}, {min.x,max.y}}};
    for (Vec2 corner : corners)
        if (pointInSector(corner, origin, direction, range, cosine)) return true;
    if (segmentIntersectsBox(origin, origin + rotated(direction, -halfArc) * range, b) ||
        segmentIntersectsBox(origin, origin + rotated(direction, halfArc) * range, b)) return true;
    for (std::size_t i = 0; i < corners.size(); ++i) {
        const Vec2 start = corners[i] - origin;
        const Vec2 delta = corners[(i + 1) % corners.size()] - corners[i];
        const float a = dot(delta, delta);
        if (a < 0.000001f) continue;
        const float linear = 2.0f * dot(start, delta);
        const float c = dot(start, start) - range * range;
        const float discriminant = linear * linear - 4.0f * a * c;
        if (discriminant < 0) continue;
        const float root = std::sqrt(discriminant);
        for (float t : {(-linear - root) / (2.0f * a), (-linear + root) / (2.0f * a)}) {
            if (t >= 0 && t <= 1 && pointInSector(corners[i] + delta * t, origin, direction, range, cosine))
                return true;
        }
    }
    return false;
}

} // namespace ashfall

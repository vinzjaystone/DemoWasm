#include "grid.h"
#include <stdexcept>

namespace ashfall {

Vec2 IsoProjection::worldToScreen(Vec2 p) const {
    return {origin.x + (p.x - p.y) * tileWidth * 0.5f,
            origin.y + (p.x + p.y) * tileHeight * 0.5f};
}
Vec2 IsoProjection::screenToWorld(Vec2 p) const {
    const float x = (p.x - origin.x) / (tileWidth * 0.5f);
    const float y = (p.y - origin.y) / (tileHeight * 0.5f);
    return {(x + y) * 0.5f, (y - x) * 0.5f};
}
TileCoord IsoProjection::screenToTile(Vec2 p) const { return tileAt(screenToWorld(p)); }
std::array<Vec2, 4> IsoProjection::tileCorners(TileCoord t) const {
    const float x = static_cast<float>(t.x), y = static_cast<float>(t.y);
    return {worldToScreen({x, y}), worldToScreen({x + 1, y}),
            worldToScreen({x + 1, y + 1}), worldToScreen({x, y + 1})};
}

Grid::Grid(int width, int height, TileKind fill) : width_(width), height_(height) {
    if (width <= 0 || height <= 0 || width > 512 || height > 512)
        throw std::invalid_argument("Grid dimensions must be between 1 and 512");
    tiles_.assign(static_cast<std::size_t>(width * height), Tile{fill, 0});
}
Grid Grid::sanctum(std::uint32_t seed) {
    Grid grid(20, 20, TileKind::Void);
    for (int y = 1; y < 19; ++y) {
        for (int x = 1; x < 19; ++x) {
            const float edge = std::abs(static_cast<float>(x) - 9.5f) +
                               std::abs(static_cast<float>(y) - 9.5f);
            if (edge < 15.1f) grid.set({x, y}, TileKind::Floor);
            grid.tiles_[static_cast<std::size_t>(y * grid.width_ + x)].variation =
                hash(static_cast<std::uint32_t>(x + y * 71) ^ seed);
        }
    }
    for (TileCoord t : {TileCoord{5, 6}, {13, 6}, {4, 12}, {14, 13}})
        grid.set(t, TileKind::Column);
    for (TileCoord t : {TileCoord{6, 7}, {7, 7}, {12, 5}, {15, 8}, {15, 9},
                        {10, 13}, {10, 14}, {5, 14}, {8, 3}, {9, 3}, {3, 9}, {12, 16}})
        grid.set(t, TileKind::Rubble);
    for (TileCoord t : {TileCoord{5, 10}, {13, 11}, {9, 6}, {11, 16}})
        grid.set(t, TileKind::Brazier);
    return grid;
}
bool Grid::contains(TileCoord t) const {
    return t.x >= 0 && t.y >= 0 && t.x < width_ && t.y < height_;
}
const Tile& Grid::at(TileCoord t) const {
    static const Tile outside{TileKind::Void, 0};
    return contains(t) ? tiles_[static_cast<std::size_t>(t.y * width_ + t.x)] : outside;
}
bool Grid::walkable(TileCoord t) const { return contains(t) && at(t).kind == TileKind::Floor; }
bool Grid::hasFloor(TileCoord t) const { return contains(t) && at(t).kind != TileKind::Void; }
void Grid::set(TileCoord t, TileKind kind) {
    if (contains(t)) tiles_[static_cast<std::size_t>(t.y * width_ + t.x)].kind = kind;
}
bool Grid::canOccupy(Vec2 p, float radius) const {
    if (!finite(p) || !std::isfinite(radius) || radius < 0.0f ||
        p.x < radius || p.y < radius ||
        p.x > static_cast<float>(width_) - radius ||
        p.y > static_cast<float>(height_) - radius) return false;
    const TileCoord min = tileAt({p.x - radius, p.y - radius});
    const TileCoord max = tileAt({p.x + radius, p.y + radius});
    for (int y = min.y; y <= max.y; ++y) {
        for (int x = min.x; x <= max.x; ++x) {
            if (walkable({x, y})) continue;
            const float closestX = std::clamp(p.x, static_cast<float>(x), static_cast<float>(x + 1));
            const float closestY = std::clamp(p.y, static_cast<float>(y), static_cast<float>(y + 1));
            if (lengthSquared(p - Vec2{closestX, closestY}) < radius * radius + 0.000001f)
                return false;
        }
    }
    return true;
}
Vec2 Grid::moveWithCollision(Vec2 from, Vec2 delta, float radius) const {
    if (!finite(from) || !finite(delta) || !std::isfinite(radius) || radius < 0.0f) return from;
    const float len = length(delta);
    if (len < 0.000001f) return from;
    // A corrupt external displacement must not create an unbounded loop.
    const int steps = std::clamp(static_cast<int>(std::ceil(std::min(len, 256.0f) / 0.10f)), 1, 2560);
    delta = normalized(delta) * std::min(len, 256.0f) / static_cast<float>(steps);
    Vec2 p = from;
    for (int i = 0; i < steps; ++i) {
        if (canOccupy(p + delta, radius)) {
            p += delta;
        } else {
            if (canOccupy({p.x + delta.x, p.y}, radius)) p.x += delta.x;
            if (canOccupy({p.x, p.y + delta.y}, radius)) p.y += delta.y;
        }
    }
    return p;
}

} // namespace ashfall

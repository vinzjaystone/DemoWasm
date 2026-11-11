#include "pathfinding.h"
#include <algorithm>
#include <array>
#include <queue>

namespace ashfall {
namespace {
float octile(TileCoord a, TileCoord b) {
    const float x = static_cast<float>(std::abs(a.x - b.x));
    const float y = static_cast<float>(std::abs(a.y - b.y));
    return std::max(x, y) + (Sqrt2 - 1.0f) * std::min(x, y);
}
struct OpenNode { int index; float g; float h; std::size_t order; };
struct Later {
    bool operator()(const OpenNode& a, const OpenNode& b) const {
        if (a.g + a.h != b.g + b.h) return a.g + a.h > b.g + b.h;
        if (a.h != b.h) return a.h > b.h;
        return a.order > b.order;
    }
};
}
PathResult Pathfinder::find(const Grid& grid, TileCoord start, TileCoord goal) {
    PathResult result;
    if (!grid.walkable(start)) { result.status = PathStatus::InvalidStart; return result; }
    if (!grid.walkable(goal)) { result.status = PathStatus::InvalidGoal; return result; }
    if (start == goal) { result.status = PathStatus::AlreadyAtGoal; return result; }
    const int width = grid.width();
    const int count = width * grid.height();
    nodes_.assign(static_cast<std::size_t>(count), Node{});
    const auto index = [width](TileCoord t) { return t.y * width + t.x; };
    const auto tile = [width](int i) { return TileCoord{i % width, i / width}; };
    std::priority_queue<OpenNode, std::vector<OpenNode>, Later> open;
    std::size_t order = 0;
    nodes_[static_cast<std::size_t>(index(start))].g = 0;
    open.push({index(start), 0, octile(start, goal), order++});
    constexpr std::array<TileCoord, 8> offsets = {{{1,0}, {0,1}, {-1,0}, {0,-1},
                                                  {1,1}, {-1,1}, {-1,-1}, {1,-1}}};
    while (!open.empty()) {
        const OpenNode entry = open.top();
        open.pop();
        Node& current = nodes_[static_cast<std::size_t>(entry.index)];
        if (current.closed || entry.g > current.g) continue;
        current.closed = true;
        ++result.visited;
        const TileCoord here = tile(entry.index);
        if (here == goal) {
            result.status = PathStatus::Found;
            for (int i = entry.index; i != index(start); i = nodes_[static_cast<std::size_t>(i)].parent)
                result.waypoints.push_back(tile(i).center());
            std::reverse(result.waypoints.begin(), result.waypoints.end());
            return result;
        }
        for (TileCoord d : offsets) {
            const TileCoord next{here.x + d.x, here.y + d.y};
            if (!grid.walkable(next)) continue;
            const bool diagonal = d.x != 0 && d.y != 0;
            if (diagonal && (!grid.walkable({here.x + d.x, here.y}) ||
                             !grid.walkable({here.x, here.y + d.y}))) continue;
            Node& neighbor = nodes_[static_cast<std::size_t>(index(next))];
            const float tentative = current.g + (diagonal ? Sqrt2 : 1.0f);
            if (neighbor.closed || tentative >= neighbor.g) continue;
            neighbor.g = tentative;
            neighbor.parent = entry.index;
            open.push({index(next), tentative, octile(next, goal), order++});
        }
    }
    return result;
}
void Navigation::assign(const PathResult& result, Vec2 position) {
    clear();
    if (!result.succeeded()) return;
    // Re-center before turning. Rerouting halfway between tiles cannot then
    // draw a diagonal through the corner of an adjacent blocked cell.
    const Vec2 center = tileAt(position).center();
    if (distance(position, center) > 0.025f) waypoints.push_back(center);
    waypoints.insert(waypoints.end(), result.waypoints.begin(), result.waypoints.end());
    if (result.status == PathStatus::AlreadyAtGoal && waypoints.empty()) waypoints.push_back(center);
}
Vec2 Navigation::advance(const Grid& grid, Vec2 p, float speed, float dt, float radius) {
    float budget = std::max(0.0f, speed * dt);
    while (moving() && budget > 0.00001f) {
        const Vec2 delta = waypoints[next] - p;
        const float remaining = length(delta);
        if (remaining < 0.025f) { p = waypoints[next++]; continue; }
        const float step = std::min(budget, remaining);
        const Vec2 moved = grid.moveWithCollision(p, delta / remaining * step, radius);
        if (distance(moved, p) < step * 0.05f) {
            clear(); // A stale path never spins forever against a wall.
            return p;
        }
        p = moved;
        budget -= step;
        if (distance(p, waypoints[next]) < 0.025f) p = waypoints[next++];
    }
    return p;
}

} // namespace ashfall

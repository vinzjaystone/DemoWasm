#pragma once

#include "grid.h"
#include <cstddef>
#include <limits>
#include <vector>

namespace ashfall {

enum class PathStatus { Found, AlreadyAtGoal, Unreachable, InvalidStart, InvalidGoal };
struct PathResult {
    PathStatus status = PathStatus::Unreachable;
    std::vector<Vec2> waypoints; // Tile centers, start excluded, goal included.
    std::size_t visited = 0;
    bool succeeded() const { return status == PathStatus::Found || status == PathStatus::AlreadyAtGoal; }
};

// Eight-connected A*, octile heuristic, no diagonal corner cutting. Working
// memory is reused between searches; all ownership is RAII and bounded by grid size.
class Pathfinder {
public:
    PathResult find(const Grid& grid, TileCoord start, TileCoord goal);
private:
    struct Node {
        float g = std::numeric_limits<float>::infinity();
        int parent = -1;
        bool closed = false;
    };
    std::vector<Node> nodes_;
};

struct Navigation {
    std::vector<Vec2> waypoints;
    std::size_t next = 0;
    void clear() { waypoints.clear(); next = 0; }
    bool moving() const { return next < waypoints.size(); }
    // Reuse a duplicate tile command only while the remaining route is still
    // collision-safe from the actor's actual position (including after knockback).
    bool canContinueTo(TileCoord goal, Vec2 currentPosition, const Grid& grid, float radius) const;
    void assign(const PathResult& result, Vec2 currentPosition, const Grid& grid, float radius);
    Vec2 advance(const Grid& grid, Vec2 position, float speed, float dt, float radius);
};

} // namespace ashfall

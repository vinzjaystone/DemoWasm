#pragma once

#include "types.h"
#include <array>
#include <vector>

namespace ashfall {

struct IsoProjection {
    float tileWidth = 64.0f;
    float tileHeight = 32.0f;
    Vec2 origin{};

    // These screen coordinates are the untransformed isometric plane. Apply the
    // Raylib camera afterwards; inverse camera transform comes before picking.
    Vec2 worldToScreen(Vec2 world) const;
    Vec2 screenToWorld(Vec2 screen) const;
    TileCoord screenToTile(Vec2 screen) const;
    std::array<Vec2, 4> tileCorners(TileCoord tile) const;
};

enum class TileKind : std::uint8_t { Void, Floor, Column, Rubble, Brazier };
struct Tile {
    TileKind kind = TileKind::Floor;
    std::uint32_t variation = 0;
};

class Grid {
public:
    explicit Grid(int width = 20, int height = 20, TileKind fill = TileKind::Floor);
    static Grid sanctum(std::uint32_t seed = 34);

    int width() const { return width_; }
    int height() const { return height_; }
    bool contains(TileCoord tile) const;
    bool walkable(TileCoord tile) const;
    bool hasFloor(TileCoord tile) const;
    const Tile& at(TileCoord tile) const;
    void set(TileCoord tile, TileKind kind);

    bool canOccupy(Vec2 position, float radius) const;
    // Swept-circle clearance along the entire segment, including tile corners.
    bool clearSegment(Vec2 from, Vec2 to, float radius) const;
    // Substepping prevents tunneling during dashes/knockback. Axis separation
    // permits sliding along obstacles, including the outside of the level.
    Vec2 moveWithCollision(Vec2 from, Vec2 displacement, float radius) const;

private:
    int width_;
    int height_;
    std::vector<Tile> tiles_;
};

} // namespace ashfall

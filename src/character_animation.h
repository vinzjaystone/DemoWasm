#pragma once

#include "player.h"
#include <cstdint>

namespace ashfall::character {

// Rows run clockwise from screen-down. Quantization happens AFTER the 2:1
// isometric projection; a tile +X direction is screen south-east, not east.
enum class Direction : std::uint8_t { South, SouthWest, West, NorthWest, North, NorthEast, East, SouthEast };
enum class Clip : std::uint8_t { Idle, Walk, Attack };
constexpr int DirectionCount = 8;
constexpr int ClipCount = 3;
constexpr float WalkCycleTiles = 2.0f;
struct ClipInfo { const char* name; int frames; int offset; };
const ClipInfo& clipInfo(Clip clip);
const char* directionName(Direction direction);
Vec2 screenDirection(Direction direction);
Vec2 worldDirection(Direction direction); // Inverse of the same 2:1 projection.
Direction quantizeDirection(Vec2 worldFacing, Direction previous = Direction::South, bool hysteresis = false);
int idleFrame(double seconds);
int walkFrame(double traveledTiles);
int attackFrame(const Attack& attack);

struct Pose {
    Clip clip = Clip::Idle;
    Direction direction = Direction::South;
    int frame = 0;
};
bool weaponBehind(const Pose& pose);
Vec2 weaponScreenDirection(const Pose& pose);
float weaponSwing(const Pose& pose); // Local, visual-only yaw; never drives melee collision.

// Presentation state only. Reads Player but never changes position, facing,
// navigation, attack state, or cooldowns. No allocation in update()/pose().
class Animator {
public:
    void reset(const Player& player);
    void update(const Player& player, float dt, bool running);
    Pose pose() const { return pose_; }
    Pose preview() const { return {Clip::Idle,Direction::SouthEast,idleFrame(idleSeconds_)}; }
    double walkDistance() const { return walkDistance_; }
    double idleSeconds() const { return idleSeconds_; }
private:
    Pose pose_{};
    Vec2 previousPosition_{};
    bool initialized_ = false;
    double idleSeconds_ = 0, walkDistance_ = 0;
};

} // namespace ashfall::character

#pragma once

#include "pathfinding.h"
#include <cstdint>

namespace ashfall {

enum class EnemyKind { Hollow, Warden };
enum class EnemyState { Idle, Chase, Attack, Dead };
struct Enemy {
    static constexpr float Radius = 0.24f;
    std::uint32_t id = 0;
    EnemyKind kind = EnemyKind::Hollow;
    EnemyState state = EnemyState::Idle;
    Vec2 position;
    Vec2 spawn;
    Vec2 facing{0, 1};
    Vec2 attackDirection{0, 1};
    Vec2 knockback{};
    Navigation navigation;
    int health = 65;
    int maxHealth = 65;
    float stateTime = 0;
    float repath = 0;
    float flash = 0;
    float animation = 0;
    std::uint32_t lastPlayerAttack = 0;
    bool attackApplied = false;
    bool alive() const { return state != EnemyState::Dead; }
    float radius() const { return kind == EnemyKind::Warden ? 0.32f : Radius; }
    float aggroRadius() const { return kind == EnemyKind::Warden ? 4.8f : 4.25f; }
    float attackRange() const { return kind == EnemyKind::Warden ? 1.4f : 1.05f; }
    float windup() const { return kind == EnemyKind::Warden ? 0.62f : 0.45f; }
    float attackInterval() const { return kind == EnemyKind::Warden ? 2.6f : 2.0f; }
    int damage() const { return kind == EnemyKind::Warden ? 24 : 14; }
    Bounds bounds() const { return {position, {radius(), radius()}}; }
};

// Exact circular-sector vs axis-aligned box intersection: corners, radial
// segments, and circle/edge intersections. Unlike a center-only angle test,
// it catches the edge of a target crossing the attack arc.
bool sectorIntersectsBox(Vec2 origin, Vec2 direction, float range, float halfArc, Bounds box);

} // namespace ashfall

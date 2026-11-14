#pragma once

#include "loot.h"
#include "pathfinding.h"
#include <cstdint>

namespace ashfall {
namespace save { struct RunCodec; }

struct Attack {
    static constexpr float HitStart = 0.075f;
    static constexpr float HitEnd = 0.24f;
    bool active = false;
    float elapsed = 0;
    float duration = 0.36f;
    float cooldown = 0;
    float comboWindow = 0;
    std::uint32_t serial = 0;
    int combo = 0;
    Vec2 direction{1, 0};
    bool damaging() const { return active && elapsed >= HitStart && elapsed <= HitEnd; }
    float range() const { return combo == 2 ? 1.85f : 1.65f; }
    float halfArc() const { return combo == 2 ? 1.30f : 1.0f; }
};

class Player {
public:
    static constexpr float Radius = 0.22f;
    static constexpr int MaxHealth = 120;
    static constexpr float BaseAttackInterval = 2.5f;
    static constexpr float AutoAttackReach = 1.30f;
    Vec2 position{8.5f, 12.5f};
    Vec2 facing{1, -0.2f};
    Vec2 knockback{};
    Navigation navigation;
    Equipment equipment;
    Backpack backpack=Backpack::starter();
    Attack attack;
    int health = MaxHealth;
    int potions = 3;
    int experience = 0;
    int embers = 0;
    float flash = 0;
    float invulnerable = 0;
    float dashRemaining = 0;
    float dashCooldown = 0;
    float animation = 0;
    bool walking = false;

    int level() const { return 1 + experience / 100; }
    int damage() const { return 26 + (level() - 1) * 3; }
    float moveSpeed() const { return 3.6f + equipment.speedBonus(); }
    float attackSpeed() const { return std::clamp(1.0f + equipment.attackSpeedBonus(),0.5f,2.0f); }
    float attackInterval() const { return BaseAttackInterval / attackSpeed(); }
    bool alive() const { return health > 0; }
    bool startAttack(Vec2 direction);
    bool dash(Vec2 direction);
    bool drinkPotion();
    int takeDamage(int rawDamage, Vec2 impulse);
    void update(const Grid& grid, float dt);
private:
    friend struct save::RunCodec;
    Vec2 dashDirection_{1, 0};
};

} // namespace ashfall

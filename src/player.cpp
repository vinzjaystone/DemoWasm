#include "player.h"

namespace ashfall {

bool Player::startAttack(Vec2 direction) {
    if (!alive() || attack.active || attack.cooldown > 0 || dashRemaining > 0) return false;
    attack.combo = attack.comboWindow > 0 ? (attack.combo + 1) % 3 : 0;
    attack.comboWindow = attackInterval() + 0.8f;
    attack.active = true;
    attack.elapsed = 0;
    attack.cooldown = attackInterval();
    attack.duration = attack.combo == 2 ? 0.43f : 0.36f;
    ++attack.serial;
    attack.direction = normalized(direction, facing);
    facing = attack.direction;
    return true;
}
bool Player::dash(Vec2 direction) {
    if (!alive() || dashCooldown > 0) return false;
    dashDirection_ = normalized(direction, facing);
    facing = dashDirection_;
    dashRemaining = 0.18f;
    dashCooldown = 2.4f;
    invulnerable = 0.27f;
    attack.active = false;
    navigation.clear();
    return true;
}
bool Player::drinkPotion() {
    if (!alive() || potions <= 0 || health == MaxHealth) return false;
    --potions;
    health = std::min(MaxHealth, health + 50);
    return true;
}
int Player::takeDamage(int rawDamage, Vec2 impulse) {
    if (!alive() || invulnerable > 0 || rawDamage <= 0) return 0;
    const int reduction = static_cast<int>(static_cast<float>(equipment.armor()) * 0.65f);
    const int applied = std::max(1, rawDamage - reduction);
    const int actual = std::min(health, applied);
    health -= actual;
    knockback += impulse;
    flash = 0.22f;
    invulnerable = 0.60f;
    if (!alive()) { navigation.clear(); attack.active = false; }
    return actual;
}
void Player::update(const Grid& grid, float dt) {
    attack.cooldown = std::max(0.0f, attack.cooldown - dt);
    attack.comboWindow = std::max(0.0f, attack.comboWindow - dt);
    dashCooldown = std::max(0.0f, dashCooldown - dt);
    invulnerable = std::max(0.0f, invulnerable - dt);
    flash = std::max(0.0f, flash - dt);
    if (attack.active) {
        attack.elapsed += dt;
        if (attack.elapsed >= attack.duration) attack.active = false;
    }
    walking = false;
    if (!alive()) return;
    if (dashRemaining > 0) {
        const float step = std::min(dt, dashRemaining);
        position = grid.moveWithCollision(position, dashDirection_ * (11.5f * step), Radius);
        dashRemaining = std::max(0.0f, dashRemaining - dt);
        animation += dt * 18;
    } else if (navigation.moving() && (!attack.active || attack.elapsed > 0.25f)) {
        const Vec2 previous = position;
        position = navigation.advance(grid, position, moveSpeed(), dt, Radius);
        const Vec2 delta = position - previous;
        walking = lengthSquared(delta) > 0.000001f;
        if (walking) { facing = normalized(delta); animation += dt * 11.0f; }
    }
    position = grid.moveWithCollision(position, knockback * dt, Radius);
    knockback = knockback * std::exp(-11.0f * dt);
}

} // namespace ashfall

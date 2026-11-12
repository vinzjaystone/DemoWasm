#include "world.h"
#include <algorithm>
#include <utility>

namespace ashfall {
namespace {
const std::vector<EnemySpawn> SanctumSpawns = {
    {{5.5f, 5.5f}}, {{8.5f, 4.5f}}, {{13.5f, 7.5f}}, {{15.5f, 11.5f}},
    {{13.5f, 14.5f}}, {{6.5f, 16.5f}}, {{4.5f, 8.5f}}, {{11.5f, 8.5f}, EnemyKind::Warden}
};
float random01(std::uint32_t seed) {
    return static_cast<float>(hash(seed) & 65535u) / 65535.0f;
}
}
World::World() : World(Grid::sanctum(), {8.5f, 12.5f}, SanctumSpawns) {}
World::World(Grid level, Vec2 playerSpawn, const std::vector<EnemySpawn>& spawns)
    : grid(std::move(level)) {
    player.position = playerSpawn;
    enemies.reserve(24);
    particles.reserve(256);
    damageNumbers.reserve(32);
    for (const EnemySpawn& spawn : spawns) spawnEnemy(spawn);
}
void World::reset() { *this = World{}; }
void World::spawnEnemy(EnemySpawn spawn) {
    const float radius = spawn.kind == EnemyKind::Warden ? 0.32f : Enemy::Radius;
    if (!grid.canOccupy(spawn.position, radius)) return;
    Enemy e;
    e.id = nextEnemyId_++;
    e.position = e.spawn = spawn.position;
    e.kind = spawn.kind;
    e.health = e.maxHealth = spawn.kind == EnemyKind::Warden ? 150 : 65;
    e.repath = static_cast<float>(e.id % 5u) * 0.08f;
    enemies.push_back(std::move(e));
    ++totalEnemies;
}
void World::notify(std::string text, float duration) {
    notice = std::move(text);
    noticeRemaining = duration;
}
bool World::movePlayerTo(Vec2 target) {
    if (!player.alive() || !finite(target)) return false;
    const TileCoord destination = tileAt(target);
    click = {destination.center(), 0.8f, false};
    PathResult path = pathfinder_.find(grid, tileAt(player.position), destination);
    lastPathVisited = path.visited;
    if (!path.succeeded()) {
        notify(grid.walkable(destination) ? "No path through the ruins." : "The way is blocked.");
        return false;
    }
    clearTarget();
    player.attack.active = false; // A new ground command interrupts the windup, not the shared cooldown.
    player.navigation.assign(path, player.position, grid, Player::Radius);
    click.valid = true;
    return true;
}
const Enemy* World::selectedEnemy() const {
    const auto found = std::find_if(enemies.begin(),enemies.end(),[this](const Enemy& enemy) {
        return enemy.id == selectedEnemyId_ && enemy.alive();
    });
    return found == enemies.end() ? nullptr : &*found;
}
bool World::selectEnemy(std::uint32_t id) {
    if (!player.alive() || id == 0) return false;
    const auto found = std::find_if(enemies.begin(),enemies.end(),[id](const Enemy& enemy) {
        return enemy.id == id && enemy.alive();
    });
    if (found == enemies.end()) return false;
    if (selectedEnemyId_ == id) return true; // Don't reset a route or swing on repeated clicks.
    selectedEnemyId_ = id;
    targetAction_ = TargetAction::Chasing;
    targetRepath_ = 0;
    targetGoal_ = {-1,-1};
    updateTargetCombat(0);
    return true;
}
void World::clearTarget() {
    if (selectedEnemyId_ != 0) player.navigation.clear();
    selectedEnemyId_ = 0;
    targetAction_ = TargetAction::None;
    targetRepath_ = 0;
    targetGoal_ = {-1,-1};
}
void World::updateTargetCombat(float dt) {
    if (selectedEnemyId_ == 0) return;
    const Enemy* enemy = selectedEnemy();
    if (!player.alive() || !enemy) { clearTarget(); return; }
    if (player.dashRemaining > 0) return;
    const Vec2 toEnemy = enemy->position-player.position;
    if (length(toEnemy) <= Player::AutoAttackReach && grid.clearSegment(player.position,enemy->position,0.04f)) {
        targetAction_ = TargetAction::Attacking;
        player.navigation.clear();
        player.facing = normalized(toEnemy,player.facing);
        player.startAttack(toEnemy); // Uses the shared ASPD cooldown, not a per-click timer.
        return;
    }
    if (targetAction_ != TargetAction::Unreachable) targetAction_ = TargetAction::Chasing;
    targetRepath_ -= dt;
    const TileCoord goal = tileAt(enemy->position);
    if (targetRepath_ > 0 || (goal == targetGoal_ && player.navigation.moving())) return;
    const PathResult path = pathfinder_.find(grid,tileAt(player.position),goal);
    lastPathVisited = path.visited;
    targetRepath_ = 0.25f;
    targetGoal_ = goal;
    if (path.succeeded()) {
        targetAction_ = TargetAction::Chasing;
        player.navigation.assign(path,player.position,grid,Player::Radius);
    } else {
        if (targetAction_ != TargetAction::Unreachable) notify("No path to that monster.");
        targetAction_ = TargetAction::Unreachable;
        player.navigation.clear();
    }
}
bool World::attackAt(Vec2 target) {
    if (!finite(target)) return false;
    clearTarget(); // A manual cleave overrides click-to-attack intent.
    return player.startAttack(target - player.position);
}
bool World::dashTo(Vec2 target) {
    if (!finite(target)) return false;
    const bool started = player.dash(target - player.position);
    if (started) { clearTarget(); burst(player.position, EffectKind::Dust, 12, 0.6f); }
    return started;
}
bool World::heal() {
    const int before = player.health;
    if (!player.drinkPotion()) {
        notify(player.potions == 0 ? "No healing flasks remain." : "Your vitality is already full.");
        return false;
    }
    damageNumbers.push_back({player.position, before - player.health, 0.9f, true, false});
    burst(player.position, EffectKind::Heal, 18);
    notify("A little light returns.");
    return true;
}
void World::cycleEquipment(EquipmentSlot slot) {
    player.equipment.cycle(slot);
    notify(std::string(player.equipment.get(slot).name) + " equipped.");
}
int World::livingEnemies() const {
    return static_cast<int>(std::count_if(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive(); }));
}
void World::update(float dt) {
    if (!std::isfinite(dt) || dt <= 0) return;
    dt = std::min(dt, 0.1f);
    if (player.alive() && !cleared()) elapsed += dt;
    screenShake = std::max(0.0f, screenShake - dt * 2.8f);
    screenFlash = std::max(0.0f, screenFlash - dt * 2.7f);
    click.remaining = std::max(0.0f, click.remaining - dt);
    noticeRemaining = std::max(0.0f, noticeRemaining - dt);
    updateEffects(dt);
    if (hitStop > 0) { hitStop = std::max(0.0f, hitStop - dt); return; }
    updateTargetCombat(dt);
    player.update(grid, dt);
    resolvePlayerAttack();
    updateEnemies(dt);
    separateEnemies(dt);
    enemies.erase(std::remove_if(enemies.begin(), enemies.end(), [](const Enemy& e) {
        return e.state == EnemyState::Dead && e.stateTime >= 0.65f;
    }), enemies.end());
}
void World::resolvePlayerAttack() {
    if (!player.alive() || !player.attack.damaging()) return;
    const Attack& attack = player.attack;
    for (Enemy& e : enemies) {
        if (!e.alive() || e.lastPlayerAttack == attack.serial) continue;
        if (!sectorIntersectsBox(player.position, attack.direction, attack.range(), attack.halfArc(), e.bounds()) ||
            !grid.clearSegment(player.position,e.position,0.04f)) continue;
        e.lastPlayerAttack = attack.serial;
        const int damage = attack.combo == 2 ? player.damage() * 3 / 2 : player.damage();
        e.health = std::max(0, e.health - damage);
        e.flash = 0.19f;
        e.knockback += normalized(e.position - player.position, attack.direction) * (attack.combo == 2 ? 6.5f : 4.2f);
        // Getting hit interrupts a telegraphed attack, not just its animation.
        e.state = EnemyState::Chase;
        e.stateTime = 0;
        e.repath = 0;
        screenShake = std::max(screenShake, attack.combo == 2 ? 0.45f : 0.2f);
        hitStop = attack.combo == 2 ? 0.045f : 0.025f;
        damageNumbers.push_back({e.position, damage, 0.85f, false, attack.combo == 2});
        burst(e.position, EffectKind::Hit, attack.combo == 2 ? 17 : 10);
        if (e.health == 0) kill(e);
    }
}
void World::kill(Enemy& e) {
    e.state = EnemyState::Dead;
    e.stateTime = 0;
    e.navigation.clear();
    ++kills;
    player.embers += e.kind == EnemyKind::Warden ? 45 : 15;
    player.experience += e.kind == EnemyKind::Warden ? 50 : 20;
    burst(e.position, EffectKind::Ember, 22, 1.2f);
    if (e.id == selectedEnemyId_) clearTarget();
    if (cleared()) notify("The sanctum is silent. Your oath endures.", 8.0f);
}
void World::updateEnemies(float dt) {
    for (Enemy& e : enemies) {
        e.flash = std::max(0.0f, e.flash - dt);
        e.stateTime += dt;
        e.animation += dt * (e.state == EnemyState::Chase ? 8.0f : 2.0f);
        if (!e.alive()) continue;
        e.position = grid.moveWithCollision(e.position, e.knockback * dt, e.radius());
        e.knockback = e.knockback * std::exp(-9.0f * dt);
        if (!player.alive()) { e.state = EnemyState::Idle; e.navigation.clear(); continue; }
        const Vec2 toPlayer = player.position - e.position;
        const float range = length(toPlayer);
        if (e.state == EnemyState::Idle) {
            if (range > e.aggroRadius()) continue;
            e.state = EnemyState::Chase;
            e.stateTime = 0;
            e.repath = 0;
        }
        if (e.state == EnemyState::Chase) {
            e.facing = normalized(toPlayer, e.facing);
            if (range < e.attackRange() && grid.clearSegment(e.position,player.position,0.04f)) {
                e.state = EnemyState::Attack;
                e.stateTime = 0;
                e.attackDirection = e.facing;
                e.attackApplied = false;
                e.navigation.clear();
            } else if (range > e.aggroRadius() + 4.0f) {
                e.state = EnemyState::Idle;
                e.navigation.clear();
            } else {
                e.repath -= dt;
                if (e.repath <= 0) {
                    PathResult path = pathfinder_.find(grid, tileAt(e.position), tileAt(player.position));
                    e.navigation.assign(path, e.position, grid, e.radius());
                    e.repath = 0.42f + static_cast<float>(e.id % 4u) * 0.065f;
                }
                // A short hit stagger stops enemies walking straight through a swing.
                if (e.flash <= 0.08f)
                    e.position = e.navigation.advance(grid, e.position,
                        e.kind == EnemyKind::Warden ? 1.55f : 2.05f, dt, e.radius());
            }
        } else if (e.state == EnemyState::Attack) {
            if (!e.attackApplied && e.stateTime >= e.windup()) {
                e.attackApplied = true;
                if (sectorIntersectsBox(e.position, e.attackDirection, e.attackRange() + 0.25f, 0.85f,
                                        {player.position, {Player::Radius, Player::Radius}}) &&
                    grid.clearSegment(e.position,player.position,0.04f)) {
                    const int actual = player.takeDamage(e.damage(), e.attackDirection * 4.0f);
                    if (actual > 0) {
                        screenShake = 0.55f;
                        screenFlash = 0.34f;
                        damageNumbers.push_back({player.position, actual, 0.85f, true, false});
                        burst(player.position, EffectKind::Hit, 12);
                        if (!player.alive()) { clearTarget(); notify("Even a fading ember can be rekindled.", 10.0f); }
                    }
                }
            }
            if (e.stateTime >= e.attackInterval()) {
                e.state = EnemyState::Chase;
                e.stateTime = 0;
                e.repath = 0;
            }
        }
    }
}
void World::separateEnemies(float dt) {
    for (std::size_t i = 0; i < enemies.size(); ++i) {
        Enemy& a = enemies[i];
        if (!a.alive()) continue;
        for (std::size_t j = i + 1; j < enemies.size(); ++j) {
            Enemy& b = enemies[j];
            if (!b.alive()) continue;
            const Vec2 delta = a.position - b.position;
            const float len = length(delta), minimum = a.radius() + b.radius() + 0.08f;
            if (len >= minimum) continue;
            const Vec2 push = normalized(delta, {1, 0}) * (minimum - len) * std::min(0.5f, dt * 6.0f);
            a.position = grid.moveWithCollision(a.position, push, a.radius());
            b.position = grid.moveWithCollision(b.position, push * -1.0f, b.radius());
        }
        // No body-blocking during an invulnerable dash.
        if (player.alive() && player.dashRemaining <= 0) {
            const Vec2 delta = a.position - player.position;
            const float len = length(delta), minimum = a.radius() + Player::Radius;
            if (len < minimum) {
                const Vec2 push = normalized(delta, {-1, 0}) * (minimum - len) * std::min(0.8f, dt * 10.0f);
                a.position = grid.moveWithCollision(a.position, push, a.radius());
            }
        }
    }
}
void World::burst(Vec2 position, EffectKind kind, int count, float power) {
    for (int i = 0; i < count && particles.size() < 512; ++i) {
        const std::uint32_t sequence = effectSequence_++;
        const float angle = random01(sequence) * 2.0f * Pi;
        const float speed = (0.3f + random01(sequence + 371u) * 1.6f) * power;
        const float life = 0.30f + random01(sequence + 71u) * 0.5f;
        particles.push_back({position, {std::cos(angle) * speed, std::sin(angle) * speed},
            12.0f, 12.0f + random01(sequence + 217u) * 35.0f,
            life, life, 1.2f + random01(sequence + 97u) * 2.2f, kind});
    }
}
void World::updateEffects(float dt) {
    for (Particle& p : particles) {
        p.remaining -= dt;
        p.position += p.velocity * dt;
        p.velocity = p.velocity * std::exp(-3.0f * dt);
        p.height += p.verticalSpeed * dt;
        p.verticalSpeed -= 85.0f * dt;
    }
    particles.erase(std::remove_if(particles.begin(), particles.end(), [](const Particle& p) {
        return p.remaining <= 0;
    }), particles.end());
    for (DamageNumber& n : damageNumbers) n.remaining -= dt;
    damageNumbers.erase(std::remove_if(damageNumbers.begin(), damageNumbers.end(), [](const DamageNumber& n) {
        return n.remaining <= 0;
    }), damageNumbers.end());
    // This bound also applies to arbitrarily long sessions with repeated healing.
    if (damageNumbers.size() > 64) damageNumbers.erase(damageNumbers.begin(), damageNumbers.end() - 64);
}

} // namespace ashfall

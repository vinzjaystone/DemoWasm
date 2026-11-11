#pragma once

#include "combat.h"
#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ashfall {

enum class EffectKind { Ember, Hit, Heal, Dust };
struct Particle {
    Vec2 position;
    Vec2 velocity;
    float height = 0;
    float verticalSpeed = 0;
    float remaining = 0;
    float lifetime = 1;
    float size = 1;
    EffectKind kind = EffectKind::Ember;
};
struct DamageNumber {
    Vec2 position;
    int amount = 0;
    float remaining = 0.85f;
    bool friendly = false;
    bool critical = false;
};
struct ClickMarker {
    Vec2 position;
    float remaining = 0;
    bool valid = false;
};
struct EnemySpawn { Vec2 position; EnemyKind kind = EnemyKind::Hollow; };

// The simulation owns every entity and effect. Rendering/input are deliberately
// separate, so this class also runs unmodified in display-independent tests.
class World {
public:
    World();
    World(Grid level, Vec2 playerSpawn, const std::vector<EnemySpawn>& spawns);
    void reset();
    void update(float dt);
    bool movePlayerTo(Vec2 target);
    bool attackAt(Vec2 target);
    bool dashTo(Vec2 target);
    bool heal();
    void cycleEquipment(EquipmentSlot slot);
    void notify(std::string text, float duration = 2.0f);
    void spawnEnemy(EnemySpawn spawn);
    int livingEnemies() const;
    bool cleared() const { return totalEnemies > 0 && kills == totalEnemies; }

    Grid grid;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<Particle> particles;
    std::vector<DamageNumber> damageNumbers;
    ClickMarker click;
    Vec2 portal{10.5f, 2.5f};
    float elapsed = 0;
    float screenShake = 0;
    float screenFlash = 0;
    float hitStop = 0;
    int kills = 0;
    int totalEnemies = 0;
    std::string notice;
    float noticeRemaining = 0;
    std::size_t lastPathVisited = 0;

private:
    Pathfinder pathfinder_;
    std::uint32_t nextEnemyId_ = 1;
    std::uint32_t effectSequence_ = 1;
    void updateEnemies(float dt);
    void resolvePlayerAttack();
    void separateEnemies(float dt);
    void burst(Vec2 position, EffectKind kind, int count, float power = 1);
    void updateEffects(float dt);
    void kill(Enemy& enemy);
};

} // namespace ashfall

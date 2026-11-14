#pragma once

#include "combat.h"
#include "player.h"
#include <cstdint>
#include <string>
#include <vector>

namespace ashfall {
namespace save { struct RunCodec; }

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
enum class TargetAction { None, Chasing, Attacking, Unreachable };

// The simulation owns every entity and effect. Rendering/input are deliberately
// separate, so this class also runs unmodified in display-independent tests.
class World {
public:
    World();
    World(Grid level, Vec2 playerSpawn, const std::vector<EnemySpawn>& spawns);
    void reset();
    void update(float dt);
    bool movePlayerTo(Vec2 target);
    bool selectEnemy(std::uint32_t id);
    void clearTarget();
    const Enemy* selectedEnemy() const;
    std::uint32_t selectedEnemyId() const { return selectedEnemyId_; }
    TargetAction targetAction() const { return targetAction_; }
    bool attackAt(Vec2 target);
    bool dashTo(Vec2 target);
    bool heal();
    void cycleEquipment(EquipmentSlot slot); // Owned spares only; never creates a catalog item.
    bool equipItem(std::uint32_t id);
    bool unequipItem(EquipmentSlot slot);
    bool dropItem(std::uint32_t id);
    bool salvageItem(std::uint32_t id);
    bool craftFlask();
    const GearItem* ownedItem(std::uint32_t id) const;
    bool wornItem(std::uint32_t id) const;
    const GroundLoot* findLoot(std::uint32_t id) const;
    const GroundLoot* nearbyLoot() const;
    bool canReachLoot(const GroundLoot& loot) const;
    bool selectLoot(std::uint32_t id);
    bool pickupLoot(std::uint32_t id);
    int pickupNearby();
    std::uint32_t selectedLootId() const { return selectedLootId_; }
    void notify(std::string text, float duration = 2.0f);
    void spawnEnemy(EnemySpawn spawn);
    int livingEnemies() const;
    bool cleared() const { return totalEnemies > 0 && kills == totalEnemies; }

    Grid grid;
    Player player;
    std::vector<Enemy> enemies;
    std::vector<GroundLoot> groundLoot;
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
    friend struct save::RunCodec;
    Pathfinder pathfinder_;
    std::uint32_t nextLootId_=7;
    std::uint32_t lootRng_=InitialLootSeed;
    std::uint32_t selectedLootId_=0;
    void clearLootTarget();
    void updateLootTarget();
    void dropEnemyLoot(const Enemy& enemy);
    void placeLoot(GroundLoot loot,Vec2 origin);
    std::uint32_t selectedEnemyId_ = 0; // IDs survive vector erasure/reallocation; never store a target pointer.
    TargetAction targetAction_ = TargetAction::None;
    float targetRepath_ = 0;
    TileCoord targetGoal_{-1,-1};
    void updateTargetCombat(float dt);
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

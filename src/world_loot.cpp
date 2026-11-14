#include "world.h"
#include <algorithm>

namespace ashfall {
const GearItem* World::ownedItem(std::uint32_t id) const {
    if (const auto* item=player.backpack.find(id)) return item;
    for (auto slot:EquipmentSlots) if (const auto* item=player.equipment.item(slot); item && item->id==id) return item;
    return nullptr;
}
bool World::wornItem(std::uint32_t id) const {
    for (auto slot:EquipmentSlots) if (const auto* item=player.equipment.item(slot); item && item->id==id) return true;
    return false;
}
void World::cycleEquipment(EquipmentSlot slot) {
    if (!player.alive()) return;
    if (player.backpack.cycleOwned(slot,player.equipment)) notify(player.equipment.item(slot)->name()+" equipped.");
    else notify("No spare for that slot. Find loot or open the backpack [B].");
}
bool World::equipItem(std::uint32_t id) {
    if (!player.alive() || !player.backpack.equip(id,player.equipment)) return false;
    notify(ownedItem(id)->name()+" equipped.");
    return true;
}
bool World::unequipItem(EquipmentSlot slot) {
    if (!player.alive()) return false;
    if (!player.backpack.unequip(slot,player.equipment)) { notify("Backpack full, or that slot is empty."); return false; }
    notify("Item moved into the backpack.");
    return true;
}
bool World::dropItem(std::uint32_t id) {
    if (!player.alive() || !player.backpack.find(id)) return false;
    if (groundLoot.size()>=MaxGroundLoot) { notify("Too much loot on the ground. Collect or salvage some first."); return false; }
    // Check all preconditions before removing anything. Dropping/recollecting
    // retains the same instance ID and rolls; it never creates a second item.
    const GearItem item=*player.backpack.find(id);
    if (findLoot(id) || wornItem(id)) return false;
    groundLoot.push_back({item.id,LootKind::Gear,player.position,1,item});
    player.backpack.remove(id);
    notify("Dropped "+item.name()+". Click it or press E nearby.");
    return true;
}
bool World::salvageItem(std::uint32_t id) {
    if (!player.alive()) return false;
    const auto* item=player.backpack.find(id);
    if (!item) return false;
    const int reward=item->salvageValue();
    if (player.embers>MaxEmbers-reward) { notify("Ember limit reached. Item kept."); return false; }
    player.backpack.remove(id); player.embers+=reward;
    notify("Salvaged for "+std::to_string(reward)+" embers.");
    return true;
}
bool World::craftFlask() {
    if (!player.alive()) return false;
    if (player.potions>=MaxFlasks) { notify("All three healing flasks are full."); return false; }
    if (player.embers<FlaskCraftCost) { notify("Crafting one flask costs 30 embers."); return false; }
    player.embers-=FlaskCraftCost; ++player.potions;
    notify("Crafted one healing flask for 30 embers.");
    return true;
}
const GroundLoot* World::findLoot(std::uint32_t id) const {
    const auto found=std::find_if(groundLoot.begin(),groundLoot.end(),[id](const GroundLoot& loot) { return loot.id==id; });
    return found==groundLoot.end()?nullptr:&*found;
}
bool World::canReachLoot(const GroundLoot& loot) const {
    return player.alive() && distance(player.position,loot.position)<=PickupRadius &&
        grid.clearSegment(player.position,loot.position,0.04f);
}
const GroundLoot* World::nearbyLoot() const {
    const GroundLoot* closest=nullptr;
    for (const auto& loot:groundLoot) if (canReachLoot(loot) && (!closest ||
        distance(player.position,loot.position)<distance(player.position,closest->position) ||
        (distance(player.position,loot.position)==distance(player.position,closest->position) && loot.id<closest->id))) closest=&loot;
    return closest;
}
void World::clearLootTarget() {
    if (selectedLootId_!=0) player.navigation.clear();
    selectedLootId_=0;
}
bool World::selectLoot(std::uint32_t id) {
    const auto* loot=findLoot(id);
    if (!loot || !player.alive()) return false;
    if (selectedLootId_==id && !canReachLoot(*loot) &&
        player.navigation.canContinueTo(tileAt(loot->position),player.position,grid,Player::Radius)) return true;
    clearLootTarget(); clearTarget();
    if (canReachLoot(*loot)) return pickupLoot(id);
    const auto path=pathfinder_.find(grid,tileAt(player.position),tileAt(loot->position));
    lastPathVisited=path.visited;
    if (!path.succeeded()) { notify("No path to that loot. It stays on the ground."); return false; }
    selectedLootId_=id;
    player.attack.active=false;
    player.navigation.assign(path,player.position,grid,Player::Radius);
    notify("Moving to "+loot->name()+"...");
    return true;
}
bool World::pickupLoot(std::uint32_t id) {
    const auto found=std::find_if(groundLoot.begin(),groundLoot.end(),[id](const auto& loot) { return loot.id==id; });
    if (found==groundLoot.end() || !canReachLoot(*found)) return false;
    switch (found->kind) {
        case LootKind::Gear:
            if (ownedItem(id) || !player.backpack.insert(found->gear)) { notify("Backpack full. Loot stays on the ground; salvage or drop an item [B].",4); return false; }
            break;
        case LootKind::Embers:
            if (player.embers>MaxEmbers-found->amount) { notify("Ember limit reached. Loot kept on the ground."); return false; }
            player.embers+=found->amount;
            break;
        case LootKind::Flask:
            if (player.potions>=MaxFlasks) { notify("Flasks full. This flask stays on the ground."); return false; }
            ++player.potions;
            break;
    }
    const std::string name=found->name();
    groundLoot.erase(found);
    if (selectedLootId_==id) clearLootTarget();
    notify("Collected "+name+".");
    return true;
}
int World::pickupNearby() {
    // Work with IDs: erase/reallocation must not leave an iterator or pointer
    // alive across a transaction. Failed gear/flasks do not block ember pickups.
    std::vector<std::uint32_t> ids;
    for (const auto& loot:groundLoot) if (canReachLoot(loot)) ids.push_back(loot.id);
    int collected=0;
    for (auto id:ids) if (pickupLoot(id)) ++collected;
    if (ids.empty()) notify("No reachable loot nearby. Click a drop to walk to it.");
    else if (collected>1) notify("Collected "+std::to_string(collected)+" drops. Unclaimed loot stays on the ground.");
    return collected;
}
void World::updateLootTarget() {
    if (selectedLootId_==0) return;
    const auto* loot=findLoot(selectedLootId_);
    if (!player.alive() || !loot) { clearLootTarget(); return; }
    if (canReachLoot(*loot)) {
        const auto id=selectedLootId_;
        pickupLoot(id); clearLootTarget(); // Full bag/flasks do not create an endless retry loop.
    } else if (!player.navigation.moving() && player.dashRemaining<=0) {
        notify("Could not get close enough to collect that drop."); clearLootTarget();
    }
}
void World::placeLoot(GroundLoot loot,Vec2 origin) {
    // Spread nearby markers without ever spawning an inaccessible drop inside
    // a column/void. The defeated actor's collision-safe feet are the fallback.
    const float angle=static_cast<float>(hash(loot.id)%628u)*0.01f;
    const Vec2 offset{std::cos(angle)*0.32f,std::sin(angle)*0.32f};
    loot.position=grid.canOccupy(origin+offset,0.04f)?origin+offset:origin;
    groundLoot.push_back(loot);
}
void World::dropEnemyLoot(const Enemy& enemy) {
    const bool warden=enemy.kind==EnemyKind::Warden;
    GearItem item=rollGear(nextLootId_++,warden,false,lootRng_);
    placeLoot({item.id,LootKind::Gear,{},1,item},enemy.position);
    placeLoot({nextLootId_++,LootKind::Embers,{},warden?45:15,{}},enemy.position);
    if (warden) {
        item=rollGear(nextLootId_++,true,true,lootRng_);
        placeLoot({item.id,LootKind::Gear,{},1,item},enemy.position);
    }
    if (warden || lootRandom(lootRng_)%4u==0)
        placeLoot({nextLootId_++,LootKind::Flask,{},1,{}},enemy.position);
}
} // namespace ashfall

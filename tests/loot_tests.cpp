#include "run_save.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>

using namespace ashfall;
namespace {
std::size_t assertions=0;
void check(bool value,const char* expression,int line) {
    ++assertions; if (!value) throw std::runtime_error(std::string("line ")+std::to_string(line)+": "+expression);
}
#define CHECK(...) check(static_cast<bool>(__VA_ARGS__),#__VA_ARGS__,__LINE__)
snapshot::Bytes encode(const World& world) {
    std::string error; auto bytes=save::encodeRun(world,error); CHECK(!bytes.empty() && error.empty()); return bytes;
}
World clone(const World& world) {
    const auto bytes=encode(world); std::string error;
    auto restored=save::decodeRun(bytes.data(),bytes.size(),error); CHECK(restored && error.empty());
    CHECK(encode(*restored)==bytes); return std::move(*restored);
}
snapshot::Bytes read(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary); CHECK(file.good());
    return {std::istreambuf_iterator<char>{file},std::istreambuf_iterator<char>{}};
}
World bulkDrops(int wardens,bool keepOriginals=false) {
    World world=keepOriginals?World{}:World{Grid::sanctum(),{8.5f,12.5f},{}};
    for (int i=0;i<wardens;++i) {
        world.spawnEnemy({{9.5f,12.5f},EnemyKind::Warden});
        world.enemies.back().health=1; // Synthetic CPU encounter fixture, not an application/debug API.
    }
    CHECK(world.attackAt({9.5f,12.5f})); world.update(0.1f);
    CHECK(world.kills==wardens);
    world.player.position={9.5f,12.5f};
    world.player.attack.active=false;
    return world;
}
void fixtureMatches(const World& generated,const char* name) {
    const auto bytes=read(std::filesystem::path{ASHFALL_SOURCE_DIR}/"tests/fixtures"/name);
    std::string error; auto fixture=save::decodeRun(bytes.data(),bytes.size(),error);
    CHECK(fixture && error.empty() && encode(*fixture)==bytes);
    CHECK(fixture->kills==generated.kills && fixture->totalEnemies==generated.totalEnemies);
    CHECK(fixture->player.backpack.cells()==generated.player.backpack.cells());
    CHECK(fixture->player.embers==generated.player.embers && fixture->player.potions==generated.player.potions);
    for (auto slot:EquipmentSlots) {
        const auto* a=fixture->player.equipment.item(slot); const auto* b=generated.player.equipment.item(slot);
        CHECK((a!=nullptr)==(b!=nullptr)); if (a && b) CHECK(*a==*b);
    }
    CHECK(fixture->groundLoot.size()==generated.groundLoot.size());
    for (std::size_t i=0;i<generated.groundLoot.size();++i) {
        const auto& a=fixture->groundLoot[i]; const auto& b=generated.groundLoot[i];
        CHECK(a.id==b.id && a.kind==b.kind && a.amount==b.amount && a.gear==b.gear);
        // Drop scatter uses libm. Do not require cosf's final rounding bit to
        // match across native OS/CPU libraries; saved floats round-trip exactly.
        CHECK(distance(a.position,b.position)<0.00001f);
    }
}
std::size_t ownership(const World& world) {
    std::set<std::uint32_t> ids;
    std::size_t gearCount=0;
    for (auto slot:EquipmentSlots) if (const auto* item=world.player.equipment.item(slot)) {
        CHECK(item->valid() && item->slot==slot && ids.insert(item->id).second); ++gearCount;
    }
    for (const auto& cell:world.player.backpack.cells()) if (cell) {
        CHECK(cell->valid() && ids.insert(cell->id).second); ++gearCount;
    }
    for (const auto& loot:world.groundLoot) {
        CHECK(loot.id && ids.insert(loot.id).second && world.grid.canOccupy(loot.position,0.04f));
        if (loot.kind==LootKind::Gear) { CHECK(loot.gear.valid() && loot.id==loot.gear.id); ++gearCount; }
    }
    CHECK(world.player.backpack.size()<=Backpack::Capacity && world.groundLoot.size()<=MaxGroundLoot);
    return gearCount;
}
void backpackTransactions() {
    Player player;
    CHECK(player.backpack.size()==3 && player.equipment.armor()==8);
    CHECK(!player.backpack.equip(999,player.equipment));
    CHECK(player.backpack.equip(4,player.equipment));
    CHECK(player.equipment.item(EquipmentSlot::Head)->id==4);
    CHECK(player.backpack.cells()[0]->id==1 && player.equipment.armor()==11);
    CHECK(!player.backpack.find(4) && player.backpack.size()==3);
    CHECK(player.backpack.unequip(EquipmentSlot::Head,player.equipment));
    CHECK(!player.equipment.item(EquipmentSlot::Head) && player.equipment.variant(EquipmentSlot::Head)==-1);
    CHECK(player.equipment.get(EquipmentSlot::Head).armor==0 && player.equipment.armor()==7);
    CHECK(player.backpack.equip(4,player.equipment) && player.backpack.size()==3);
    for (std::uint32_t id=20;player.backpack.size()<Backpack::Capacity;++id) CHECK(player.backpack.insert({id,EquipmentSlot::Boots,2}));
    const auto before=player.backpack.cells();
    CHECK(!player.backpack.unequip(EquipmentSlot::Head,player.equipment));
    CHECK(player.backpack.cells()==before && player.equipment.item(EquipmentSlot::Head)->id==4);
    CHECK(player.backpack.equip(1,player.equipment)); // No spare cell is needed for a swap.
    CHECK(player.backpack.size()==24 && player.backpack.cells()[0]->id==4);
    CHECK(!player.backpack.insert({900,EquipmentSlot::Head,2}));
    CHECK(!player.backpack.place(24,{901,EquipmentSlot::Head,2}));
    CHECK(!player.backpack.insert({0,EquipmentSlot::Head,0}));
    CHECK(!player.backpack.cycleOwned(static_cast<EquipmentSlot>(99),player.equipment));
    CHECK(!player.backpack.unequip(static_cast<EquipmentSlot>(99),player.equipment));
    auto removed=player.backpack.remove(4); CHECK(removed && removed->id==4 && !player.backpack.remove(4));
    CHECK(!player.backpack.place(0,*player.backpack.cells()[1])); // Duplicate ID, even in a free cell.
    CHECK(player.backpack.place(0,*removed));
    auto sorted=player.backpack; sorted.sort(); const auto once=sorted.cells(); sorted.sort(); CHECK(sorted.cells()==once);
    Equipment empty;
    for (auto slot:EquipmentSlots) CHECK(empty.set(slot,std::nullopt));
    CHECK(empty.armor()==0 && empty.speedBonus()==0 && empty.attackSpeedBonus()==0);
    CHECK(!empty.set(EquipmentSlot::Boots,GearItem{10,EquipmentSlot::Head,0}));
}
void ownedShortcutsAndCooldowns() {
    World world;
    CHECK(world.attackAt(world.player.position+Vec2{1,0}));
    const auto attack=world.player.attack;
    const auto count=ownership(world);
    for (int i=0;i<30;++i) {
        for (auto slot:EquipmentSlots) world.cycleEquipment(slot);
        CHECK(ownership(world)==count);
        CHECK(world.player.attack.cooldown==attack.cooldown && world.player.attack.serial==attack.serial);
        CHECK(world.player.equipment.variant(EquipmentSlot::Head)!=2); // Unowned crown cannot materialize.
        CHECK(world.player.equipment.variant(EquipmentSlot::Armor)!=0);
        CHECK(world.player.equipment.variant(EquipmentSlot::Boots)!=2);
    }
    world.player.backpack=Backpack{};
    const auto id=world.player.equipment.item(EquipmentSlot::Head)->id;
    world.cycleEquipment(EquipmentSlot::Head);
    CHECK(world.player.equipment.item(EquipmentSlot::Head)->id==id && world.player.backpack.size()==0);
    world.player.takeDamage(100000,{});
    CHECK(!world.equipItem(id) && !world.unequipItem(EquipmentSlot::Head) && !world.craftFlask());
}
void rollsAndTables() {
    std::uint32_t a=InitialLootSeed,b=a;
    std::array<int,3> rarities{};
    for (std::uint32_t id=7;id<10007;++id) {
        const auto one=rollGear(id,false,false,a),two=rollGear(id,false,false,b);
        CHECK(one==two && one.valid()); ++rarities[static_cast<std::size_t>(one.rarity)];
        CHECK(one.armor()>=one.base().armor && one.speedBonus()>=one.base().speedBonus && one.attackSpeedBonus()>=one.base().attackSpeedBonus);
    }
    CHECK(rarities[0]>6500 && rarities[1]>2100 && rarities[2]>200);
    for (std::uint32_t i=1;i<500;++i) {
        const auto boss=rollGear(i,true,true,a); CHECK(boss.valid() && boss.rarity==Rarity::Relic);
        const auto bonus=rollGear(i,true,false,a); CHECK(bonus.valid() && bonus.rarity!=Rarity::Common);
    }
    GearItem bad{1,EquipmentSlot::Head,0};
    bad.armorRoll=1; CHECK(!bad.valid()); bad.rarity=Rarity::Tempered; CHECK(!bad.valid());
    bad.speedRoll=5; bad.hasteRoll=1; CHECK(bad.valid()); bad.hasteRoll=4; CHECK(!bad.valid());
    auto world=bulkDrops(1);
    CHECK(world.groundLoot.size()==4 && world.player.embers==0 && world.player.experience==50);
    CHECK(std::count_if(world.groundLoot.begin(),world.groundLoot.end(),[](const auto& loot) { return loot.kind==LootKind::Gear && loot.gear.rarity==Rarity::Relic; })>=1);
    CHECK(std::count_if(world.groundLoot.begin(),world.groundLoot.end(),[](const auto& loot) { return loot.kind==LootKind::Embers && loot.amount==45; })==1);
    CHECK(ownership(world)==8);
    const auto count=world.groundLoot.size();
    for (int i=0;i<2000;++i) world.update(0.1f);
    CHECK(world.enemies.empty() && world.groundLoot.size()==count); // Neither corpse fade nor time destroys drops.
    auto max=bulkDrops(64);
    CHECK(max.groundLoot.size()==256 && ownership(max)==134);
    max.spawnEnemy({{9.5f,12.5f}}); CHECK(max.totalEnemies==64);
    CHECK(encode(max).size()<save::MaxRunBytes);
    CHECK(ownership(clone(max))==134);
}
void pickupsAndEconomy() {
    World world;
    const GearItem original=*world.player.backpack.find(4);
    CHECK(world.dropItem(4) && !world.player.backpack.find(4));
    CHECK(world.findLoot(4) && world.findLoot(4)->gear==original && !world.dropItem(4));
    CHECK(world.pickupLoot(4) && !world.findLoot(4));
    CHECK(*world.player.backpack.find(4)==original && !world.pickupLoot(4));
    CHECK(!world.dropItem(1) && !world.salvageItem(1)); // Worn gear must be unequipped first.
    CHECK(!world.craftFlask());
    world.player.potions=2; world.player.embers=29;
    CHECK(!world.craftFlask() && world.player.embers==29 && world.player.potions==2);
    world.player.embers=30; CHECK(world.craftFlask() && world.player.potions==3 && world.player.embers==0);
    CHECK(!world.craftFlask());
    CHECK(world.salvageItem(4) && world.player.embers==original.salvageValue());
    CHECK(!world.salvageItem(4) && !world.player.backpack.find(4));
    world.player.embers=MaxEmbers; CHECK(!world.salvageItem(5) && world.player.backpack.find(5));
    auto crowded=bulkDrops(25);
    CHECK(crowded.pickupNearby()>0 && crowded.player.backpack.size()==24);
    CHECK(crowded.player.embers==1125 && crowded.player.potions==3);
    const auto flask=std::find_if(crowded.groundLoot.begin(),crowded.groundLoot.end(),[](const auto& loot) { return loot.kind==LootKind::Flask; })->id;
    CHECK(!crowded.pickupLoot(flask) && crowded.findLoot(flask));
    crowded.player.potions=2; CHECK(crowded.pickupLoot(flask) && !crowded.findLoot(flask) && crowded.player.potions==3);
    const auto gear=std::find_if(crowded.groundLoot.begin(),crowded.groundLoot.end(),[](const auto& loot) { return loot.kind==LootKind::Gear; })->id;
    CHECK(!crowded.pickupLoot(gear) && crowded.findLoot(gear));
    CHECK(crowded.selectLoot(gear)==false && crowded.selectedLootId()==0); // Failure does not create a retry loop.
    const auto space=crowded.player.backpack.cells()[0]->id;
    CHECK(crowded.dropItem(space) && crowded.pickupLoot(gear));
    CHECK(crowded.player.backpack.size()==24 && crowded.findLoot(space));
    CHECK(ownership(crowded)==56); // Six starters + fifty drops, no hidden duplicates.
}
void navigationAndWalls() {
    World world(Grid(12,12),{3.5f,3.5f},{{{8.5f,8.5f}}});
    CHECK(world.dropItem(4));
    world.player.position={7.5f,3.5f};
    CHECK(world.selectEnemy(world.enemies.front().id));
    CHECK(world.selectLoot(4) && world.selectedEnemyId()==0 && world.selectedLootId()==4);
    const auto route=world.player.navigation.waypoints; const auto cursor=world.player.navigation.next;
    for (int i=0;i<12;++i) {
        CHECK(world.selectLoot(4) && world.player.navigation.waypoints.size()==route.size() && world.player.navigation.next==cursor);
        CHECK(std::equal(route.begin(),route.end(),world.player.navigation.waypoints.begin(),[](Vec2 a,Vec2 b) { return a.x==b.x && a.y==b.y; }));
    }
    CHECK(world.movePlayerTo({7.5f,4.5f}) && world.selectedLootId()==0);
    CHECK(world.selectLoot(4)); CHECK(world.attackAt({8,4}) && world.selectedLootId()==0);
    CHECK(world.selectLoot(4)); CHECK(world.dashTo({9,4}) && world.selectedLootId()==0);
    world.player.dashRemaining=0; world.player.navigation.clear();
    CHECK(world.selectLoot(4));
    for (int i=0;i<1000 && world.findLoot(4);++i) world.update(1.0f/120);
    CHECK(!world.findLoot(4) && world.player.backpack.find(4));
    World corner(Grid(12,12),{5.76f,6.24f},{});
    corner.grid.set({6,6},TileKind::Column);
    CHECK(corner.grid.canOccupy(corner.player.position,Player::Radius));
    corner.groundLoot.push_back({4,LootKind::Gear,{6.24f,5.76f},1,{4,EquipmentSlot::Head,1}});
    corner.player.backpack.remove(4);
    CHECK(distance(corner.player.position,corner.groundLoot.front().position)<PickupRadius);
    CHECK(!corner.pickupLoot(4) && corner.pickupNearby()==0); // Close is not enough through a blocked corner.
    CHECK(corner.selectLoot(4));
    for (int i=0;i<1000 && corner.findLoot(4);++i) corner.update(1.0f/120);
    CHECK(!corner.findLoot(4));
    World unreachable(Grid(12,12),{3.5f,3.5f},{});
    CHECK(unreachable.dropItem(4)); unreachable.player.position={8.5f,3.5f};
    for (int y=0;y<12;++y) unreachable.grid.set({6,y},TileKind::Column);
    CHECK(!unreachable.selectLoot(4) && unreachable.findLoot(4) && unreachable.selectedLootId()==0);
}
void checkpointAndLegacy() {
    const auto legacy=read(std::filesystem::path{ASHFALL_SOURCE_DIR}/"tests/fixtures/legacy-run-v1.ashrun");
    std::string error; auto migrated=save::decodeRun(legacy.data(),legacy.size(),error);
    CHECK(migrated && migrated->player.health==67 && migrated->player.potions==1 && migrated->player.experience==140);
    CHECK(migrated->player.embers==30 && migrated->elapsed==47.25f && migrated->kills==1);
    CHECK(migrated->player.backpack.size()==0 && migrated->groundLoot.empty());
    for (auto slot:EquipmentSlots) CHECK(migrated->player.equipment.variant(slot)==2 && migrated->player.equipment.item(slot)->rarity==Rarity::Common);
    CHECK(clone(*migrated).player.backpack.size()==0);
    for (int i=0;i<100;++i) migrated->update(1.0f/120);
    CHECK(migrated->groundLoot.empty()); // Old immediate currency is not paid twice.
    World world;
    CHECK(world.dropItem(4)); world.player.position={12.5f,12.5f};
    CHECK(world.selectLoot(4)); auto resumed=clone(world);
    CHECK(resumed.selectedLootId()==4 && resumed.player.navigation.moving());
    for (int i=0;i<400;++i) { world.update(1.0f/120); resumed.update(1.0f/120); CHECK(encode(world)==encode(resumed)); }
    CHECK(resumed.player.backpack.find(4) && !resumed.findLoot(4));
    World before;
    before.enemies.front().position={9.5f,12.5f}; before.enemies.front().health=1;
    auto after=clone(before);
    // Consume cosmetic-effect RNG in only one run; gear must still match.
    before.player.health=70; CHECK(before.heal());
    CHECK(before.attackAt({9.5f,12.5f}) && after.attackAt({9.5f,12.5f}));
    before.update(0.1f); after.update(0.1f);
    CHECK(before.groundLoot.size()==after.groundLoot.size());
    for (std::size_t i=0;i<before.groundLoot.size();++i) {
        CHECK(before.groundLoot[i].kind==after.groundLoot[i].kind && before.groundLoot[i].gear==after.groundLoot[i].gear);
    }
    auto restored=clone(before);
    CHECK(ownership(restored)==ownership(before));
    fixtureMatches(bulkDrops(1),"victory-loot-v2.ashrun");
}
void hostileInventorySaves() {
    const auto bytes=encode(World{});
    const auto legacySize=read(std::filesystem::path{ASHFALL_SOURCE_DIR}/"tests/fixtures/legacy-run-v1.ashrun").size();
    const std::size_t extra=legacySize,worn=extra+12+1,bag=extra+12+3*29+1;
    CHECK(bytes.size()==extra+12+3*29+24+3*28+4);
    const auto put=[](snapshot::Bytes& data,std::size_t offset,std::uint32_t value) {
        CHECK(offset+4<=data.size()); for (unsigned i=0;i<4;++i) data[offset+i]=static_cast<unsigned char>((value>>(8*i))&255);
    };
    const auto reject=[&](snapshot::Bytes bad) {
        put(bad,16,snapshot::checksum(bad.data()+20,bad.size()-20));
        std::string error; CHECK(!save::decodeRun(bad.data(),bad.size(),error) && !error.empty());
    };
    for (const auto& mutation:std::initializer_list<std::pair<std::size_t,std::uint32_t>>{
        {extra,0},{extra,9999999},{extra+4,0},{extra+8,4},{worn,0},{worn+4,2},{worn+8,2},
        {worn+12,3},{worn+16,1},{worn+20,31},{worn+24,7},{bag,1},{bag+4,3},{bag+12,2},{bag+16,0xffffffffu},
        {bytes.size()-4,0xffffffffu}}) {
        auto bad=bytes; put(bad,mutation.first,mutation.second); reject(bad);
    }
    auto bad=bytes; bad[extra+12]=2; reject(bad); // Strict optional markers.
    bad=bytes; bad[112]=255; reject(bad); // Legacy variant prefix must match the v2 instance graph.
    World dropped; CHECK(dropped.dropItem(4)); const auto ground=encode(dropped);
    const std::size_t record=ground.size()-48;
    for (const auto& mutation:std::initializer_list<std::pair<std::size_t,std::uint32_t>>{
        {record,1},{record+4,3},{record+8,0x7fc00000u},{record+12,0x7f800000u},{record+16,0},{record+20,6}}) {
        bad=ground; put(bad,mutation.first,mutation.second); reject(bad);
    }
    World invalid; invalid.groundLoot.resize(MaxGroundLoot+1); std::string error;
    CHECK(save::encodeRun(invalid,error).empty());
    invalid=World{}; invalid.groundLoot.push_back({4,LootKind::Gear,invalid.player.position,1,{4,EquipmentSlot::Head,1}});
    CHECK(save::encodeRun(invalid,error).empty()); // Bag/ground alias rejected atomically.
    auto fixture=bulkDrops(25,true); fixture.pickupNearby();
    CHECK(fixture.player.backpack.size()==24 && clone(fixture).player.backpack.size()==24);
    fixtureMatches(fixture,"full-backpack-v2.ashrun");
}
void transactionStress() {
    auto world=bulkDrops(64);
    std::uint32_t random=0x12942u;
    std::size_t count=ownership(world);
    for (int i=0;i<2000;++i) {
        const auto command=lootRandom(random)%8u;
        const auto& cells=world.player.backpack.cells();
        const auto& cell=cells[lootRandom(random)%Backpack::Capacity];
        const auto id=cell?cell->id:0;
        switch (command) {
            case 0: world.pickupNearby(); break;
            case 1: world.equipItem(id); break;
            case 2: world.unequipItem(static_cast<EquipmentSlot>(lootRandom(random)%3u)); break;
            case 3: world.dropItem(id); break;
            case 4: if (world.salvageItem(id)) --count; break;
            case 5: world.player.backpack.sort(); break;
            case 6: world.cycleEquipment(static_cast<EquipmentSlot>(lootRandom(random)%3u)); break;
            case 7: world.craftFlask(); break;
        }
        CHECK(ownership(world)==count);
        if (i%50==0) world=clone(world);
    }
}
}
int main(int argc,char** argv) {
    if (argc==3 && (std::string(argv[1])=="--write-fixture" || std::string(argv[1])=="--write-victory-fixture")) {
        const bool victory=std::string(argv[1])=="--write-victory-fixture";
        auto world=victory?bulkDrops(1):bulkDrops(25,true);
        if (!victory) world.pickupNearby();
        const auto bytes=encode(world);
        std::ofstream file(argv[2],std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        return file?0:1;
    }
    int failures=0;
    for (const auto& test:std::initializer_list<std::pair<const char*,std::function<void()>>>{
        {"bounded backpack and atomic swaps",backpackTransactions},{"owned shortcuts preserve attack cooldowns",ownedShortcutsAndCooldowns},
        {"deterministic rarity rolls and bounded enemy tables",rollsAndTables},{"ground ownership, capacity, flasks and ember economy",pickupsAndEconomy},
        {"pickup navigation, cancellation and wall rejection",navigationAndWalls},{"checkpoint continuation and v1 migration",checkpointAndLegacy},
        {"hostile inventory/loot saves",hostileInventorySaves},{"2000 mixed transactions with checkpoint replay",transactionStress}}) {
        try { test.second(); std::cout<<"[PASS] "<<test.first<<'\n'; }
        catch (const std::exception& error) { ++failures; std::cerr<<"[FAIL] "<<test.first<<": "<<error.what()<<'\n'; }
    }
    std::cout<<assertions<<" assertions, "<<failures<<" failures\n"; return failures?1:0;
}

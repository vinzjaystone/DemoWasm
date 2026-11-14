#include "run_save.h"
#include <algorithm>
#include <utility>
#include <array>
#include <set>

namespace ashfall::save {
namespace {
using snapshot::Reader;
using snapshot::Writer;
constexpr std::size_t MaxEnemies=64,MaxWaypoints=400;
constexpr float LongTimer=1.0e9f;
void vector(Writer& out,Vec2 v) { out.number(v.x); out.number(v.y); }
Vec2 vector(Reader& in) { const float x=in.number(),y=in.number(); return {x,y}; }
int integer(Reader& in,int maximum) {
    const auto value=in.u32();
    if (value>static_cast<std::uint32_t>(maximum)) { in.ok=false; return 0; }
    return static_cast<int>(value);
}
float number(Reader& in,float minimum,float maximum) {
    const float value=in.number(); if (value<minimum || value>maximum) in.ok=false; return value;
}
Vec2 direction(Reader& in) {
    const Vec2 value=vector(in); const float norm=lengthSquared(value);
    if (norm<0.25f || norm>2.25f) in.ok=false;
    return value;
}
Vec2 impulse(Reader& in) {
    const Vec2 value=vector(in); if (lengthSquared(value)>10000) in.ok=false; return value;
}
Vec2 position(Reader& in,const Grid& grid,float radius) {
    const Vec2 value=vector(in);
    if (!in.ok || !grid.canOccupy(value,radius)) in.ok=false;
    return value;
}
void navigation(Writer& out,const Navigation& route) {
    out.u32(static_cast<std::uint32_t>(route.waypoints.size())); out.u32(static_cast<std::uint32_t>(route.next));
    for (Vec2 point:route.waypoints) vector(out,point);
}
Navigation navigation(Reader& in,const Grid& grid,float radius) {
    Navigation route;
    const auto count=in.u32(),next=in.u32();
    if (count>MaxWaypoints || next>count || !in.take(static_cast<std::size_t>(count)*8)) { in.ok=false; return route; }
    route.next=next; route.waypoints.reserve(count);
    for (std::uint32_t i=0;i<count;++i) {
        const Vec2 point=position(in,grid,radius);
        // Only A* tile centers belong in a navigation route. Do not demand
        // clearance from the actor: legitimate knockback can displace it.
        const Vec2 center=tileAt(point).center();
        if (distance(point,center)>0.001f) in.ok=false;
        if (!route.waypoints.empty() && in.ok && !grid.clearSegment(route.waypoints.back(),point,radius)) in.ok=false;
        route.waypoints.push_back(point);
    }
    return route;
}
std::uint32_t mapId(const Grid& grid) {
    Writer out; out.u32(static_cast<std::uint32_t>(grid.width())); out.u32(static_cast<std::uint32_t>(grid.height()));
    for (int y=0;y<grid.height();++y) for (int x=0;x<grid.width();++x) {
        const auto& tile=grid.at({x,y}); out.u8(static_cast<unsigned char>(tile.kind)); out.u32(tile.variation);
    }
    return snapshot::checksum(out.bytes.data(),out.bytes.size());
}
}

// This is the only serialization access to private simulation state. Loading
// builds a separate World; neither decoding nor validation touches the live one.
struct RunCodec {
    static snapshot::Bytes write(const World& world) {
        Writer out;
        out.u32(1); // Rules/catalog revision, in addition to the envelope version.
        out.u32(mapId(world.grid));
        out.number(world.elapsed); out.number(world.hitStop);
        out.u32(static_cast<std::uint32_t>(world.kills)); out.u32(static_cast<std::uint32_t>(world.totalEnemies));
        out.u32(world.nextEnemyId_); out.u32(world.effectSequence_);
        out.u32(world.selectedEnemyId_); out.u32(static_cast<std::uint32_t>(world.targetAction_));
        out.number(world.targetRepath_);
        out.u32(static_cast<std::uint32_t>(world.targetGoal_.x)+1u); out.u32(static_cast<std::uint32_t>(world.targetGoal_.y)+1u);
        const auto& player=world.player;
        vector(out,player.position); vector(out,player.facing); vector(out,player.knockback); vector(out,player.dashDirection_);
        navigation(out,player.navigation);
        for (auto slot:{EquipmentSlot::Head,EquipmentSlot::Armor,EquipmentSlot::Boots}) out.u8(static_cast<unsigned char>(player.equipment.variant(slot)));
        for (int value:{player.health,player.potions,player.experience,player.embers}) out.u32(static_cast<std::uint32_t>(value));
        for (float value:{player.flash,player.invulnerable,player.dashRemaining,player.dashCooldown,player.animation}) out.number(value);
        out.boolean(player.walking);
        const Attack& attack=player.attack;
        out.boolean(attack.active);
        for (float value:{attack.elapsed,attack.duration,attack.cooldown,attack.comboWindow}) out.number(value);
        out.u32(attack.serial); out.u32(static_cast<std::uint32_t>(attack.combo)); vector(out,attack.direction);
        out.u32(static_cast<std::uint32_t>(world.enemies.size()));
        for (const auto& enemy:world.enemies) {
            out.u32(enemy.id); out.u32(static_cast<std::uint32_t>(enemy.kind)); out.u32(static_cast<std::uint32_t>(enemy.state));
            vector(out,enemy.position); vector(out,enemy.spawn); vector(out,enemy.facing); vector(out,enemy.attackDirection); vector(out,enemy.knockback);
            navigation(out,enemy.navigation);
            out.u32(static_cast<std::uint32_t>(enemy.health)); out.u32(static_cast<std::uint32_t>(enemy.maxHealth));
            for (float value:{enemy.stateTime,enemy.repath,enemy.flash,enemy.animation}) out.number(value);
            out.u32(enemy.lastPlayerAttack); out.boolean(enemy.attackApplied);
        }
        return snapshot::wrap(RunMagic,out.bytes);
    }
    static std::optional<World> read(Reader& in) {
        World world;
        if (in.u32()!=1 || in.u32()!=mapId(world.grid)) return std::nullopt;
        world.elapsed=number(in,0,LongTimer); world.hitStop=number(in,0,0.1f);
        world.kills=integer(in,static_cast<int>(MaxEnemies)); world.totalEnemies=integer(in,static_cast<int>(MaxEnemies));
        world.nextEnemyId_=in.u32(); world.effectSequence_=in.u32();
        world.selectedEnemyId_=in.u32(); world.targetAction_=static_cast<TargetAction>(integer(in,3));
        world.targetRepath_=number(in,-LongTimer,1);
        world.targetGoal_={integer(in,world.grid.width())-1,integer(in,world.grid.height())-1};
        auto& player=world.player;
        player.position=position(in,world.grid,Player::Radius); player.facing=direction(in);
        player.knockback=impulse(in); player.dashDirection_=direction(in);
        player.navigation=navigation(in,world.grid,Player::Radius);
        for (auto slot:{EquipmentSlot::Head,EquipmentSlot::Armor,EquipmentSlot::Boots}) {
            const int variant=in.u8();
            if (variant>=static_cast<int>(Equipment::VariantsPerSlot)) { in.ok=false; break; }
            while (player.equipment.variant(slot)!=variant) player.equipment.cycle(slot);
        }
        player.health=integer(in,Player::MaxHealth); player.potions=integer(in,3);
        player.experience=integer(in,10000000); player.embers=integer(in,10000000);
        player.flash=number(in,0,1); player.invulnerable=number(in,0,1);
        player.dashRemaining=number(in,0,0.18f); player.dashCooldown=number(in,0,2.4f); player.animation=number(in,0,LongTimer);
        player.walking=in.boolean();
        Attack& attack=player.attack;
        attack.active=in.boolean(); attack.elapsed=number(in,0,1); attack.duration=number(in,0.35f,0.44f);
        attack.cooldown=number(in,0,5); attack.comboWindow=number(in,0,6);
        attack.serial=in.u32(); attack.combo=integer(in,2); attack.direction=direction(in);
        if ((attack.active && (attack.elapsed>=attack.duration || !player.alive() || player.dashRemaining>0)) ||
            (player.dashRemaining>0 && player.dashCooldown<=0)) in.ok=false;
        const auto count=in.u32();
        if (!in.ok || count>MaxEnemies) return std::nullopt;
        world.enemies.clear(); std::set<std::uint32_t> ids;
        for (std::uint32_t i=0;i<count;++i) {
            Enemy enemy;
            enemy.id=in.u32(); enemy.kind=static_cast<EnemyKind>(integer(in,1)); enemy.state=static_cast<EnemyState>(integer(in,3));
            enemy.position=position(in,world.grid,enemy.radius()); enemy.spawn=position(in,world.grid,enemy.radius());
            enemy.facing=direction(in); enemy.attackDirection=direction(in); enemy.knockback=impulse(in);
            enemy.navigation=navigation(in,world.grid,enemy.radius());
            enemy.health=integer(in,150); enemy.maxHealth=integer(in,150);
            enemy.stateTime=number(in,0,LongTimer); enemy.repath=number(in,-LongTimer,1);
            enemy.flash=number(in,0,1); enemy.animation=number(in,0,LongTimer);
            enemy.lastPlayerAttack=in.u32(); enemy.attackApplied=in.boolean();
            if (enemy.id==0 || enemy.id>=world.nextEnemyId_ || !ids.insert(enemy.id).second ||
                enemy.maxHealth!=(enemy.kind==EnemyKind::Warden?150:65) || enemy.health>enemy.maxHealth ||
                (enemy.health>0)!=enemy.alive()) in.ok=false;
            world.enemies.push_back(std::move(enemy));
        }
        const bool goalUnset=world.targetGoal_==TileCoord{-1,-1};
        if (!in.done() || world.totalEnemies<world.kills || world.livingEnemies()!=world.totalEnemies-world.kills ||
            world.enemies.size()>static_cast<std::size_t>(world.totalEnemies) ||
            world.nextEnemyId_!=static_cast<std::uint32_t>(world.totalEnemies)+1u ||
            ((world.selectedEnemyId_==0)!=(world.targetAction_==TargetAction::None)) ||
            (world.selectedEnemyId_!=0 && (!world.selectedEnemy() || !player.alive())) ||
            (!goalUnset && !world.grid.walkable(world.targetGoal_))) return std::nullopt;
        return world;
    }
};

snapshot::Bytes encodeRun(const World& world,std::string& error) {
    error.clear();
    const Grid level=Grid::sanctum();
    if (world.grid.width()!=level.width() || world.grid.height()!=level.height() || mapId(world.grid)!=mapId(level) ||
        world.portal.x!=10.5f || world.portal.y!=2.5f || world.enemies.size()>MaxEnemies ||
        world.player.navigation.waypoints.size()>MaxWaypoints || world.player.navigation.next>MaxWaypoints ||
        std::any_of(world.enemies.begin(),world.enemies.end(),[](const Enemy& enemy) {
            return enemy.navigation.waypoints.size()>MaxWaypoints || enemy.navigation.next>MaxWaypoints;
        })) { error="This run does not match the bundled sanctum or exceeds save limits."; return {}; }
    auto bytes=RunCodec::write(world);
    if (bytes.size()>MaxRunBytes || !decodeRun(bytes.data(),bytes.size(),error)) {
        if (error.empty()) error="This run could not be saved safely.";
        return {};
    }
    return bytes;
}
std::optional<World> decodeRun(const unsigned char* bytes,std::size_t size,std::string& error) {
    error.clear();
    auto reader=snapshot::unwrap(bytes,size,RunMagic,MaxRunBytes);
    if (reader.ok) { auto world=RunCodec::read(reader); if (world) return world; }
    error="Invalid, corrupt, or incompatible .ashrun save (128 KiB maximum). Current run kept.";
    return std::nullopt;
}
} // namespace ashfall::save

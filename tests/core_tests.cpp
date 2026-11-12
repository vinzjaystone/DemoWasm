#include "world.h"
#include "raylib_compat.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ashfall;
namespace {
int assertions = 0;
void check(bool condition, const char* expression, int line) {
    ++assertions;
    if (!condition) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(...) check(static_cast<bool>(__VA_ARGS__), #__VA_ARGS__, __LINE__)
bool near(float a, float b, float tolerance = 0.001f) { return std::abs(a - b) <= tolerance; }
void tick(World& world, float seconds) {
    for (int i = 0; i < static_cast<int>(std::ceil(seconds * 120.0f)); ++i) world.update(1.0f / 120.0f);
}
void raylibGradientSignatures() {
    // Stand-ins exercise both API shapes without installing/linking Raylib.
    struct Tint { unsigned char r, g, b, a; };
    struct Capture {
        Vec2 center{};
        float radius = 0;
        Tint inner{}, outer{};
        int calls = 0;
    } legacy, modern;
    const auto legacyDraw = [&](int x, int y, float radius, Tint inner, Tint outer) {
        legacy = {{static_cast<float>(x), static_cast<float>(y)}, radius, inner, outer, legacy.calls + 1};
    };
    const auto modernDraw = [&](Vec2 center, float radius, Tint inner, Tint outer) {
        modern = {center, radius, inner, outer, modern.calls + 1};
    };
    const Tint inner{255, 155, 65, 24}, outer{68, 75, 57, 0};
    const auto sameTint = [](Tint a, Tint b) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    for (Vec2 center : {Vec2{12.75f, -4.25f}, {-13.5f, 22.75f}}) {
        raylib_compat::drawCircleGradient(legacyDraw, center, 36.5f, inner, outer);
        raylib_compat::drawCircleGradient(modernDraw, center, 36.5f, inner, outer);
        CHECK(legacy.center.x == static_cast<float>(static_cast<int>(center.x)));
        CHECK(legacy.center.y == static_cast<float>(static_cast<int>(center.y)));
        CHECK(modern.center.x == center.x && modern.center.y == center.y);
        CHECK(legacy.radius == 36.5f && modern.radius == 36.5f);
        CHECK(sameTint(legacy.inner, inner) && sameTint(modern.inner, inner));
        CHECK(sameTint(legacy.outer, outer) && sameTint(modern.outer, outer));
    }
    CHECK(legacy.calls == 2 && modern.calls == 2);
}
void projection() {
    IsoProjection iso{64, 32, {123, -71}};
    for (Vec2 p : {Vec2{0, 0}, {4.5f, 8.5f}, {-3.25f, 9.1f}, {20.0f, -10.0f}}) {
        const Vec2 result = iso.screenToWorld(iso.worldToScreen(p));
        CHECK(near(result.x, p.x));
        CHECK(near(result.y, p.y));
    }
    CHECK(iso.screenToTile(iso.worldToScreen({-0.01f, 2.9f})) == TileCoord{-1, 2});
    CHECK(iso.screenToTile(iso.worldToScreen({6.9f, 7.1f})) == TileCoord{6, 7});
    CHECK(tileAt({std::numeric_limits<float>::infinity(), 0}) == TileCoord{-1, -1});
    CHECK(tileAt({1e30f, -1e30f}) == TileCoord{1048576, -1048576});
    const auto corners = iso.tileCorners({3, 4});
    CHECK(near(corners[1].x - corners[3].x, 64));
    CHECK(near(corners[2].y - corners[0].y, 32));
}
void gridAndCollision() {
    Grid grid(8, 8);
    CHECK(!grid.walkable({-1, 0}));
    CHECK(!grid.walkable({8, 0}));
    CHECK(grid.at({-1, 0}).kind == TileKind::Void);
    grid.set({4, 3}, TileKind::Column);
    CHECK(!grid.canOccupy({3.9f, 3.5f}, 0.22f));
    CHECK(grid.canOccupy({3.7f, 3.5f}, 0.22f));
    CHECK(!grid.canOccupy({0.1f, 1.5f}, 0.22f));
    CHECK(!grid.canOccupy({std::numeric_limits<float>::quiet_NaN(), 0}, 0.22f));
    const Vec2 blocked = grid.moveWithCollision({3.5f, 3.5f}, {5, 0}, 0.22f);
    CHECK(blocked.x < 3.79f); // A fast dash cannot tunnel through the column.
    CHECK(grid.canOccupy(blocked, 0.22f));
    const Vec2 slide = grid.moveWithCollision({3.6f, 3.2f}, {0.7f, -0.6f}, 0.22f);
    CHECK(slide.y < 2.8f);
    CHECK(grid.canOccupy(slide, 0.22f));
    bool threw = false;
    try { Grid invalid(0, 8); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}
void pathBasics() {
    Pathfinder finder;
    Grid grid(8, 8);
    auto path = finder.find(grid, {1, 1}, {6, 6});
    CHECK(path.status == PathStatus::Found);
    CHECK(path.waypoints.size() == 5);
    CHECK(tileAt(path.waypoints.back()) == TileCoord{6, 6});
    CHECK(path.visited <= 64);
    CHECK(finder.find(grid, {1, 1}, {1, 1}).status == PathStatus::AlreadyAtGoal);
    CHECK(finder.find(grid, {-1, 0}, {1, 1}).status == PathStatus::InvalidStart);
    grid.set({6, 6}, TileKind::Rubble);
    CHECK(finder.find(grid, {1, 1}, {6, 6}).status == PathStatus::InvalidGoal);
    CHECK(finder.find(grid, {1, 1}, {100, 100}).status == PathStatus::InvalidGoal);
}
void pathCornersAndMaze() {
    Pathfinder finder;
    Grid corner(3, 3, TileKind::Void);
    corner.set({0, 0}, TileKind::Floor);
    corner.set({1, 1}, TileKind::Floor);
    CHECK(finder.find(corner, {0, 0}, {1, 1}).status == PathStatus::Unreachable);
    Grid maze(10, 10);
    for (int y = 0; y < 9; ++y) maze.set({5, y}, TileKind::Column);
    const auto path = finder.find(maze, {2, 2}, {8, 2});
    CHECK(path.succeeded());
    bool usedGap = false;
    TileCoord previous{2, 2};
    for (Vec2 point : path.waypoints) {
        const TileCoord next = tileAt(point);
        CHECK(maze.walkable(next));
        CHECK(std::abs(previous.x - next.x) <= 1);
        CHECK(std::abs(previous.y - next.y) <= 1);
        if (next.x != previous.x && next.y != previous.y) {
            CHECK(maze.walkable({previous.x, next.y}));
            CHECK(maze.walkable({next.x, previous.y}));
        }
        if (next.y == 9) usedGap = true;
        previous = next;
    }
    CHECK(usedGap);
    maze.set({5, 9}, TileKind::Column);
    CHECK(finder.find(maze, {2, 2}, {8, 2}).status == PathStatus::Unreachable);
    // Reusing the same workspace after a failed search must not retain closed nodes.
    CHECK(finder.find(Grid(4, 4), {0, 0}, {3, 3}).waypoints.size() == 3);
}
// Independent Dijkstra reference checks optimal cost across deterministic maps.
void optimalPaths() {
    Pathfinder finder;
    for (std::uint32_t seed = 1; seed <= 80; ++seed) {
        Grid grid(9,9);
        for (int y = 0; y < 9; ++y) for (int x = 0; x < 9; ++x)
            if (hash(static_cast<std::uint32_t>(x+y*9)^seed*73u)%5u==0)
                grid.set({x,y},TileKind::Rubble);
        grid.set({0,0},TileKind::Floor);
        grid.set({8,8},TileKind::Floor);
        std::vector<float> costs(81,std::numeric_limits<float>::infinity());
        using Entry = std::pair<float,int>;
        std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
        costs[0] = 0;
        queue.push({0,0});
        while (!queue.empty()) {
            const auto [cost,index] = queue.top();
            queue.pop();
            if (cost>costs[static_cast<std::size_t>(index)]) continue;
            const TileCoord at{index%9,index/9};
            for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
                if (dx==0 && dy==0) continue;
                const TileCoord next{at.x+dx,at.y+dy};
                if (!grid.walkable(next)) continue;
                if (dx!=0 && dy!=0 && (!grid.walkable({at.x+dx,at.y}) || !grid.walkable({at.x,at.y+dy}))) continue;
                const float candidate = cost+(dx!=0 && dy!=0?Sqrt2:1.0f);
                const auto nextIndex = static_cast<std::size_t>(next.y*9+next.x);
                if (candidate<costs[nextIndex]) {
                    costs[nextIndex] = candidate;
                    queue.push({candidate,static_cast<int>(nextIndex)});
                }
            }
        }
        const auto result = finder.find(grid,{0,0},{8,8});
        CHECK(result.succeeded()==std::isfinite(costs.back()));
        CHECK(result.visited<=81);
        if (result.succeeded()) {
            Vec2 previous{0.5f,0.5f};
            float cost = 0;
            for (Vec2 point : result.waypoints) { cost += distance(previous,point); previous = point; }
            CHECK(near(cost,costs.back(),0.0001f));
            const auto again = finder.find(grid,{0,0},{8,8});
            CHECK(again.waypoints.size()==result.waypoints.size());
            for (std::size_t i = 0; i < result.waypoints.size(); ++i)
                CHECK(distance(again.waypoints[i],result.waypoints[i])<0.0001f);
        }
    }
}
void navigation() {
    Grid grid(10, 10);
    Pathfinder finder;
    Navigation nav;
    Vec2 p{1.5f, 1.5f};
    nav.assign(finder.find(grid, {1, 1}, {6, 6}), p, grid, Player::Radius);
    p = nav.advance(grid, p, 20, 1, Player::Radius);
    CHECK(!nav.moving());
    CHECK(near(p.x, 6.5f));
    CHECK(near(p.y, 6.5f));
    p = {2.7f, 2.5f};
    nav.assign(finder.find(grid, {2, 2}, {2, 6}), p, grid, Player::Radius);
    CHECK(near(nav.waypoints.front().x, 2.5f));
    CHECK(near(nav.waypoints.front().y, 3.5f)); // New route, not the current tile center.
    const Vec2 rerouted = nav.advance(grid,p,3.0f,1.0f/120.0f,Player::Radius);
    CHECK(rerouted.y>p.y && rerouted.x<p.x);
    p=rerouted;
    for (int i = 0; i < 300; ++i) p = nav.advance(grid, p, 3.0f, 1.0f / 120.0f, Player::Radius);
    CHECK(!nav.moving());
    CHECK(near(p.y, 6.5f));
}
void smoothRetargeting() {
    World world(Grid(20,8),{1.5f,2.5f},{});
    CHECK(world.movePlayerTo({17.5f,2.5f}));
    for (int frame = 0; frame < 120; ++frame) {
        if (frame%3==0) CHECK(world.movePlayerTo({frame%2==0?17.5f:16.5f,2.5f}));
        const Vec2 before = world.player.position;
        world.update(1.0f/120.0f);
        CHECK(world.player.position.x > before.x);
        CHECK(near(world.player.position.y,2.5f));
    }
    CHECK(near(world.player.position.x,1.5f+world.player.moveSpeed(),0.03f));
    CHECK(world.movePlayerTo({12.5f,5.5f}));
    const Vec2 turn = world.player.position;
    world.update(1.0f/120.0f);
    CHECK(world.player.position.x > turn.x && world.player.position.y > turn.y);

    Grid corner(8,8);
    corner.set({3,1},TileKind::Column);
    const Vec2 start{2.75f,2.08f}, end{3.5f,2.5f};
    CHECK(corner.canOccupy(start,Player::Radius));
    CHECK(corner.canOccupy(end,Player::Radius));
    CHECK(!corner.clearSegment(start,end,Player::Radius)); // Mid-segment corner grazing.
    CHECK(!corner.clearSegment(end,start,Player::Radius));
    CHECK(!corner.clearSegment(start,end,-1));
    CHECK(!corner.clearSegment({NAN,2.5f},end,Player::Radius));
    CHECK(!corner.clearSegment({1.5f,1.5f},{5.5f,1.5f},0));
    CHECK(corner.clearSegment({1.5f,4.5f},{5.5f,4.5f},Player::Radius));
    Pathfinder finder;
    Navigation nav;
    nav.assign(finder.find(corner,tileAt(start),{6,2}),start,corner,Player::Radius);
    CHECK(distance(nav.waypoints.front(),tileAt(start).center())<0.001f);
    Vec2 p = start;
    for (int i = 0; i < 240; ++i) {
        p=nav.advance(corner,p,3.6f,1.0f/120.0f,Player::Radius);
        CHECK(corner.canOccupy(p,Player::Radius));
    }
    CHECK(distance(p,TileCoord{6,2}.center())<0.03f);
}
// Repeating a ground command must be idempotent at the tile level, including
// mid-turn, when entering the goal tile, and when an obstacle forces a detour.
void repeatedGroundCommands() {
    for (int scenario=0;scenario<3;++scenario) {
        Grid grid=scenario==2 ? Grid::sanctum() : Grid(20,20);
        const Vec2 spawn=scenario==2 ? Vec2{8.5f,12.5f} : Vec2{1.5f,1.5f};
        const TileCoord goal=scenario==0 ? TileCoord{7,4} : (scenario==1 ? TileCoord{14,2} : TileCoord{15,5});
        World once(grid,spawn,{}), repeated=once;
        CHECK(once.movePlayerTo(goal.center()));
        CHECK(repeated.movePlayerTo(goal.center()));
        bool enteredGoal=false;
        for (int frame=0;frame<1800;++frame) {
            const Vec2 position=repeated.player.position;
            if (tileAt(position)==goal && repeated.player.navigation.moving()) enteredGoal=true;
            if (frame%3==0) {
                // Different points inside the same diamond are still the same command.
                const Vec2 target=goal.center()+Vec2{frame%2==0?-0.3f:0.3f,frame%2==0?0.2f:-0.2f};
                const auto next=repeated.player.navigation.next;
                const auto waypoints=repeated.player.navigation.waypoints;
                CHECK(repeated.movePlayerTo(target));
                CHECK(distance(repeated.player.position,position)<0.000001f);
                CHECK(repeated.lastPathVisited==0);
                if (next<waypoints.size()) {
                    CHECK(repeated.player.navigation.next==next);
                    CHECK(repeated.player.navigation.waypoints.size()==waypoints.size());
                    for (std::size_t i=0;i<waypoints.size();++i)
                        CHECK(distance(repeated.player.navigation.waypoints[i],waypoints[i])<0.000001f);
                }
            }
            once.update(1.0f/120.0f);
            repeated.update(1.0f/120.0f);
            CHECK(distance(once.player.position,repeated.player.position)<0.000001f);
            CHECK(grid.canOccupy(repeated.player.position,Player::Radius));
        }
        CHECK(enteredGoal);
        CHECK(!repeated.player.navigation.moving());
        CHECK(distance(repeated.player.position,goal.center())<0.001f);
        // Clicking again after arrival must not create a fake movement step.
        CHECK(repeated.movePlayerTo(goal.center()));
        CHECK(!repeated.player.navigation.moving());
    }
}
void occupiedTileCommands() {
    World world(Grid(12,10),{3.8f,3.5f},{});
    CHECK(world.movePlayerTo({9.5f,3.5f}));
    world.update(1.0f/120.0f);
    CHECK(world.player.position.x>3.8f);
    const Vec2 stop=world.player.position;
    CHECK(tileAt(stop)==TileCoord{3,3});
    for (int i=0;i<120;++i) {
        CHECK(world.movePlayerTo({3.2f,3.8f}));
        CHECK(world.click.valid && !world.player.navigation.moving());
        world.update(1.0f/120.0f);
        CHECK(distance(world.player.position,stop)<0.000001f);
    }
    CHECK(world.movePlayerTo({9.5f,3.5f}));
    CHECK(world.player.navigation.moving()); // A different destination still works.

    World combat(Grid(12,10),{3.8f,3.5f},{{{4.7f,3.5f}}});
    CHECK(combat.selectEnemy(combat.enemies.front().id));
    CHECK(combat.player.attack.active);
    const float cooldown=combat.player.attack.cooldown;
    CHECK(combat.movePlayerTo({3.1f,3.1f}));
    CHECK(!combat.selectedEnemy() && combat.selectedEnemyId()==0);
    CHECK(!combat.player.attack.active && !combat.player.navigation.moving());
    CHECK(combat.player.attack.cooldown==cooldown); // Stop intent never resets ASPD.
}
void groundRouteInvalidation() {
    World world(Grid(12,10),{1.5f,3.5f},{});
    const Vec2 goal{9.5f,3.5f};
    CHECK(world.movePlayerTo(goal));
    tick(world,0.1f);
    const auto next=world.player.navigation.next;
    const auto count=world.player.navigation.waypoints.size();
    // Rejecting a different, invalid command must leave the active route intact.
    CHECK(!world.movePlayerTo({-1,3.5f}) && !world.click.valid);
    CHECK(world.player.navigation.next==next && world.player.navigation.waypoints.size()==count);
    CHECK(world.movePlayerTo(goal) && world.lastPathVisited==0);

    // A duplicate destination is not permission to keep a stale, blocked path.
    // The first leg is still clear; this obstacle is farther along the route.
    world.grid.set({5,3},TileKind::Column);
    CHECK(world.movePlayerTo(goal));
    CHECK(world.lastPathVisited>0);
    CHECK(world.player.navigation.canContinueTo(tileAt(goal),world.player.position,world.grid,Player::Radius));
    for (int i=0;i<600;++i) {
        world.update(1.0f/120.0f);
        CHECK(world.grid.canOccupy(world.player.position,Player::Radius));
    }
    CHECK(distance(world.player.position,goal)<0.001f);

    // Knockback can put the actor on the other side of a wall from its next
    // waypoint even when all remaining waypoint centers are walkable.
    World displaced(Grid(12,10),{1.5f,3.5f},{});
    CHECK(displaced.movePlayerTo(goal));
    displaced.grid.set({2,2},TileKind::Column);
    displaced.player.position={2.5f,1.5f};
    CHECK(!displaced.player.navigation.canContinueTo(tileAt(goal),displaced.player.position,displaced.grid,Player::Radius));
    CHECK(displaced.movePlayerTo(goal) && displaced.lastPathVisited>0);
    tick(displaced,4);
    CHECK(distance(displaced.player.position,goal)<0.001f);
    CHECK(displaced.dashTo({10.5f,3.5f}));
    CHECK(!displaced.player.navigation.moving());
    tick(displaced,0.3f);
    CHECK(displaced.movePlayerTo(goal)); // Clearing the route cannot leave a cached goal behind.
    CHECK(displaced.player.navigation.moving());

    // A ground click on the chase's goal tile is a new intent, not a duplicate
    // ground command. Cancel selection and recovery must not be bypassed.
    World chasing(Grid(14,10),{1.5f,4.5f},{{{9.5f,4.5f}}});
    CHECK(chasing.selectEnemy(chasing.enemies.front().id));
    CHECK(chasing.movePlayerTo({9.2f,4.8f}));
    CHECK(chasing.selectedEnemyId()==0 && chasing.player.navigation.moving());
    CHECK(chasing.lastPathVisited>0);
    CHECK(!chasing.player.attack.active && chasing.player.attack.serial==0);
}
void targetedCombat() {
    World world(Grid(14,10),{1.5f,4.5f},{{{9.5f,4.5f}}});
    const auto id = world.enemies.front().id;
    CHECK(world.selectEnemy(id));
    CHECK(world.selectedEnemyId()==id && world.selectedEnemy()!=nullptr);
    CHECK(world.targetAction()==TargetAction::Chasing && world.player.navigation.moving());
    CHECK(world.player.attack.serial==0); // Do not swing at a distant target.
    for (int i = 0; i < 600 && world.player.attack.serial==0; ++i) world.update(1.0f/120.0f);
    CHECK(world.player.attack.serial==1);
    CHECK(distance(world.player.position,world.selectedEnemy()->position)<=Player::AutoAttackReach+0.05f);
    CHECK(!world.player.navigation.moving());
    const auto serial = world.player.attack.serial;
    for (int i = 0; i < 120; ++i) {
        CHECK(world.selectEnemy(id));
        world.update(1.0f/120.0f);
    }
    CHECK(world.player.attack.serial==serial);
    for (int i = 0; i < 240 && world.player.attack.serial==serial; ++i) world.update(1.0f/120.0f);
    CHECK(world.player.attack.serial==serial+1);
    CHECK(world.selectedEnemy()->health<world.selectedEnemy()->maxHealth);
    CHECK(world.movePlayerTo({1.5f,1.5f}));
    CHECK(!world.selectedEnemy() && world.selectedEnemyId()==0);
    CHECK(!world.player.attack.active);
    CHECK(world.player.navigation.moving());
    CHECK(!world.selectEnemy(99999));
    CHECK(world.selectEnemy(id));
    CHECK(world.dashTo({1.5f,1.5f}));
    CHECK(!world.selectedEnemy());

    World unreachable(Grid(8,8),{1.5f,3.5f},{{{5.5f,3.5f}}});
    for (int y=0;y<8;++y) unreachable.grid.set({3,y},TileKind::Column);
    CHECK(unreachable.selectEnemy(unreachable.enemies.front().id));
    tick(unreachable,1);
    CHECK(unreachable.targetAction()==TargetAction::Unreachable);
    CHECK(!unreachable.player.navigation.moving() && unreachable.player.attack.serial==0);
    unreachable.grid.set({3,3},TileKind::Floor); // A path becoming available resumes the same selection.
    tick(unreachable,0.3f);
    CHECK(unreachable.targetAction()!=TargetAction::Unreachable);
    CHECK(unreachable.selectedEnemy()!=nullptr);

    World dead(Grid(8,8),{2.5f,2.5f},{{{3.4f,2.5f}}});
    const auto deadId=dead.enemies.front().id;
    dead.enemies.front().health=1;
    CHECK(dead.selectEnemy(deadId));
    tick(dead,0.3f);
    CHECK(dead.kills==1 && !dead.selectedEnemy() && dead.selectedEnemyId()==0);
    tick(dead,1);
    CHECK(dead.enemies.empty() && !dead.selectEnemy(deadId));
}
void targetIdentityAndAttackSpeed() {
    World world(Grid(12,12),{2.5f,2.5f},{{{3.4f,2.5f}},{{2.5f,3.4f}}});
    const auto first=world.enemies[0].id, second=world.enemies[1].id;
    CHECK(world.selectEnemy(first));
    const float cooldown=world.player.attack.cooldown;
    const auto serial=world.player.attack.serial;
    CHECK(world.selectEnemy(second));
    CHECK(world.selectedEnemyId()==second && world.player.attack.serial==serial);
    CHECK(world.player.attack.cooldown==cooldown);
    for (int i=0;i<50;++i) world.spawnEnemy({{9.5f,9.5f}}); // Force vector reallocation.
    CHECK(world.selectedEnemy()->id==second);
    world.enemies[0].state=EnemyState::Dead;
    world.enemies[0].stateTime=1;
    tick(world,0.1f);
    CHECK(world.selectedEnemy()->id==second); // Erase a different vector element.
    world.player.takeDamage(10000,{0,0});
    tick(world,0.1f);
    CHECK(world.selectedEnemyId()==0);
    world.reset();
    CHECK(!world.selectedEnemy());

    Player player;
    CHECK(near(player.attackInterval(),2.5f));
    player.equipment.cycle(EquipmentSlot::Boots); // Greaves: slower ASPD.
    CHECK(player.attackInterval()>2.5f);
    player.equipment.cycle(EquipmentSlot::Boots); // Fleetstriders: faster ASPD.
    CHECK(player.attackInterval()<2.5f);
    CHECK(player.startAttack({1,0}));
    CHECK(near(player.attack.cooldown,player.attackInterval()));
    const auto oldCooldown=player.attack.cooldown;
    player.equipment.cycle(EquipmentSlot::Armor);
    CHECK(player.attack.cooldown==oldCooldown); // Gear cannot reset recovery mid-swing.
}
void targetRepathAndOcclusion() {
    World moving(Grid(14,10),{1.5f,4.5f},{{{9.5f,4.5f}}});
    const auto id=moving.enemies.front().id;
    CHECK(moving.selectEnemy(id));
    tick(moving,0.1f);
    moving.enemies.front().position={9.5f,7.5f};
    tick(moving,0.3f);
    CHECK(moving.selectedEnemyId()==id);
    CHECK(near(moving.player.navigation.waypoints.back().y,7.5f));
    CHECK(moving.player.attack.serial==0);
    for (int i=0;i<1200 && moving.player.attack.serial==0;++i) moving.update(1.0f/120.0f);
    CHECK(moving.player.attack.serial==1); // Follow the moved monster without another click.

    Grid corner(8,8);
    corner.set({3,2},TileKind::Column);
    World blocked(corner,{2.65f,2.4f},{{{3.35f,3.4f}}});
    CHECK(distance(blocked.player.position,blocked.enemies.front().position)<Player::AutoAttackReach);
    CHECK(blocked.selectEnemy(blocked.enemies.front().id));
    CHECK(blocked.targetAction()==TargetAction::Chasing && blocked.player.attack.serial==0);
    CHECK(blocked.attackAt(blocked.enemies.front().position));
    CHECK(blocked.selectedEnemyId()==0);
    // Hold the enemy in recovery to isolate line-of-sight from its navigation.
    blocked.enemies.front().state=EnemyState::Attack;
    blocked.enemies.front().attackApplied=true;
    tick(blocked,0.3f);
    CHECK(blocked.enemies.front().health==blocked.enemies.front().maxHealth);
}
void attackCadence() {
    Grid grid(20,20);
    float fastest=10, slowest=0;
    for (int head=0;head<3;++head) for (int armor=0;armor<3;++armor) for (int boots=0;boots<3;++boots) {
        Player player;
        for (int i=0;i<head;++i) player.equipment.cycle(EquipmentSlot::Head);
        for (int i=0;i<armor;++i) player.equipment.cycle(EquipmentSlot::Armor);
        for (int i=0;i<boots;++i) player.equipment.cycle(EquipmentSlot::Boots);
        const float interval=player.attackInterval();
        CHECK(interval>=1.999f && interval<=3.0f);
        fastest=std::min(fastest,interval);
        slowest=std::max(slowest,interval);
        CHECK(player.startAttack({1,0}));
        int frames=0;
        do {
            player.update(grid,1.0f/120.0f);
            ++frames;
        } while (!player.startAttack({1,0}) && frames<400);
        const float actual=static_cast<float>(frames)/120.0f;
        CHECK(player.attack.serial==2 && player.attack.combo==1);
        CHECK(actual>=interval-0.001f && actual<=interval+0.009f);
    }
    CHECK(near(fastest,2.0f) && near(slowest,2.5f/0.9f));
}
void wardrobe() {
    Equipment equipment;
    CHECK(equipment.armor() == 8);
    const int starting = equipment.variant(EquipmentSlot::Head);
    const int armor = equipment.armor();
    equipment.cycle(EquipmentSlot::Head);
    CHECK(equipment.armor() > armor);
    CHECK(equipment.variant(EquipmentSlot::Armor) == 1);
    equipment.cycle(EquipmentSlot::Head);
    equipment.cycle(EquipmentSlot::Head);
    CHECK(equipment.variant(EquipmentSlot::Head) == starting);
    const float speed = equipment.speedBonus();
    equipment.cycle(EquipmentSlot::Boots);
    equipment.cycle(EquipmentSlot::Boots);
    CHECK(equipment.speedBonus() > speed);
}
void attackGeometry() {
    const Vec2 origin{0, 0}, right{1, 0};
    CHECK(sectorIntersectsBox(origin, right, 2, 0.9f, {{1.2f, 0}, {0.2f, 0.2f}}));
    CHECK(!sectorIntersectsBox(origin, right, 2, 0.9f, {{-1.2f, 0}, {0.2f, 0.2f}}));
    CHECK(!sectorIntersectsBox(origin, right, 2, 0.9f, {{3, 0}, {0.2f, 0.2f}}));
    CHECK(sectorIntersectsBox(origin, right, 2, 0.9f, {{2.15f, 0}, {0.2f, 0.2f}}));
    CHECK(sectorIntersectsBox(origin, right, 2, 0.2f, {{1.0f, 0.45f}, {0.1f, 0.3f}}));
    // Arc grazes a long box whose corners are all outside the circle.
    CHECK(sectorIntersectsBox(origin, right, 2, 0.2f, {{2.05f, 0}, {0.1f, 2.5f}}));
    // Sector is wholly inside the target box.
    CHECK(sectorIntersectsBox(origin, right, 2, 0.9f, {{0, 0}, {5, 5}}));
    CHECK(!sectorIntersectsBox(origin, right, 0, 0.9f, {{0, 0}, {1, 1}}));
    CHECK(!sectorIntersectsBox(origin, right, 2, 0, {{0, 0}, {1, 1}}));
}
void playerTimersAndArmor() {
    Player player;
    CHECK(player.startAttack({1, 0}));
    CHECK(!player.startAttack({1, 0}));
    CHECK(near(player.attackInterval(),2.5f));
    for (int i = 0; i < 60; ++i) player.update(Grid(20, 20), 1.0f / 120.0f);
    CHECK(!player.attack.active);
    CHECK(!player.startAttack({1,0})); // Finishing an animation does not reset ASPD recovery.
    for (int i = 0; i < 252; ++i) player.update(Grid(20,20),1.0f/120.0f);
    CHECK(player.startAttack({1, 0}));
    CHECK(player.attack.combo == 1);
    const int damage = player.takeDamage(14, {0, 0});
    CHECK(damage > 0 && damage < 14);
    CHECK(player.takeDamage(14, {0, 0}) == 0);
    CHECK(player.drinkPotion());
    CHECK(player.health == Player::MaxHealth);
    CHECK(player.potions == 2);
    CHECK(!player.drinkPotion());
    CHECK(player.potions == 2);
    CHECK(player.dash({1, 0}));
    CHECK(!player.attack.active);
    CHECK(!player.dash({1, 0}));
    CHECK(player.takeDamage(100, {0, 0}) == 0);
}
void oncePerSwingAndDeath() {
    World world(Grid(12, 12), {3.5f, 3.5f}, {{{4.6f, 3.5f}}});
    CHECK(world.totalEnemies == 1);
    CHECK(world.attackAt({5, 3.5f}));
    tick(world, 0.3f);
    CHECK(world.enemies.front().health == 65 - world.player.damage());
    CHECK(world.kills == 0);
    // A single selection finishes the encounter without holding an attack key.
    CHECK(world.selectEnemy(world.enemies.front().id));
    for (int i = 0; i < 1200 && !world.cleared(); ++i) world.update(1.0f/120.0f);
    CHECK(world.kills == 1);
    CHECK(world.cleared());
    CHECK(world.player.embers == 15);
    tick(world, 1.0f);
    CHECK(world.enemies.empty());
}
void enemyAiAndDodge() {
    World world(Grid(12, 12), {5.5f, 5.5f}, {{{8.5f, 5.5f}}});
    tick(world, 0.1f);
    CHECK(world.enemies.front().state == EnemyState::Chase);
    tick(world, 2.4f);
    CHECK(world.player.health < Player::MaxHealth);
    CHECK(world.grid.canOccupy(world.player.position, Player::Radius));
    World dodge(Grid(12, 12), {5.5f, 5.5f}, {{{6.3f, 5.5f}}});
    tick(dodge, 0.1f);
    CHECK(dodge.enemies.front().state == EnemyState::Attack);
    CHECK(dodge.dashTo({2.0f, 5.5f}));
    tick(dodge, 0.7f);
    CHECK(dodge.player.health == Player::MaxHealth);
    CHECK(dodge.grid.canOccupy(dodge.player.position, Player::Radius));
}
void deathAndFullRun() {
    World death(Grid(10,10),{4.5f,4.5f},{{{5.4f,4.5f}}});
    CHECK(death.movePlayerTo({3.5f,4.5f}));
    CHECK(death.player.takeDamage(10000,{0,0})==Player::MaxHealth);
    CHECK(!death.player.alive());
    CHECK(!death.player.navigation.moving());
    CHECK(!death.attackAt({5.4f,4.5f}));
    CHECK(!death.movePlayerTo({3.5f,4.5f}));
    CHECK(!death.dashTo({1,1}));
    CHECK(!death.heal());
    const float deathTime = death.elapsed;
    tick(death,1.0f);
    CHECK(death.elapsed==deathTime);
    CHECK(death.enemies.front().state==EnemyState::Idle);

    // Complete the authored arena using the public gameplay commands, not
    // teleports, modified health, or forced kills. This also exercises the Warden.
    World run;
    for (int frame = 0; frame < 36000 && run.player.alive() && !run.cleared(); ++frame) {
        if (!run.selectedEnemy()) {
            const Enemy* target = nullptr;
            float closest = 1e9f;
            for (const Enemy& enemy : run.enemies) {
                const float d = distance(run.player.position,enemy.position);
                if (enemy.alive() && d<closest) { closest=d; target=&enemy; }
            }
            CHECK(target!=nullptr);
            CHECK(run.selectEnemy(target->id));
        }
        if (run.player.health<65 && run.player.potions>0) run.heal();
        run.update(1.0f/120.0f);
    }
    CHECK(run.cleared());
    CHECK(run.kills==8 && run.livingEnemies()==0);
    CHECK(run.player.alive());
    CHECK(run.player.embers==150);
    CHECK(run.player.level()==2);
    const float clearTime = run.elapsed;
    tick(run,1.0f);
    CHECK(run.elapsed==clearTime);
    CHECK(run.enemies.empty());
    run.reset();
    CHECK(run.kills==0 && run.livingEnemies()==8 && run.player.level()==1);
}
void levelAndLongRun() {
    World world;
    CHECK(world.totalEnemies == 8);
    CHECK(world.grid.canOccupy(world.player.position, Player::Radius));
    Pathfinder finder;
    CHECK(finder.find(world.grid, tileAt(world.player.position), tileAt(world.portal)).succeeded());
    for (const Enemy& e : world.enemies) {
        CHECK(world.grid.canOccupy(e.position, e.radius()));
        CHECK(finder.find(world.grid, tileAt(world.player.position), tileAt(e.position)).succeeded());
    }
    // Every floor tile in the authored level must be reachable from the spawn.
    for (int y = 0; y < world.grid.height(); ++y)
        for (int x = 0; x < world.grid.width(); ++x)
            if (world.grid.walkable({x, y}))
                CHECK(finder.find(world.grid, tileAt(world.player.position), {x, y}).succeeded());
    world.player.invulnerable = 1000;
    for (int i = 0; i < 12000; ++i) {
        if (i % 120 == 0) world.movePlayerTo({2.5f + static_cast<float>((i / 120) % 14), 11.5f});
        if (i % 55 == 0) world.attackAt({10, 10});
        if (i % 300 == 0) world.dashTo({10, 10});
        world.update(1.0f / 120.0f);
        CHECK(world.grid.canOccupy(world.player.position, Player::Radius));
        for (const Enemy& e : world.enemies) CHECK(world.grid.canOccupy(e.position, e.radius()));
        CHECK(world.particles.size() <= 512);
        CHECK(world.damageNumbers.size() <= 64);
    }
    world.reset();
    CHECK(world.player.health == Player::MaxHealth);
    CHECK(world.player.potions == 3);
    CHECK(world.kills == 0 && world.totalEnemies == 8);
    CHECK(world.enemies.size() == 8);
    CHECK(world.particles.empty());
}
}
int main() {
    const std::vector<std::pair<const char*, std::function<void()>>> tests = {
        {"Raylib 5.5/6.0 gradient API compatibility", raylibGradientSignatures},
        {"projection and negative picking", projection}, {"grid collision and tunneling", gridAndCollision},
        {"A* basic cases", pathBasics}, {"A* maze and no corner cutting", pathCornersAndMaze},
        {"A* optimality vs Dijkstra on 80 maps", optimalPaths},
        {"continuous rerouting and swept corner clearance", smoothRetargeting},
        {"idempotent repeated destination clicks", repeatedGroundCommands},
        {"occupied-tile clicks stop without recentering", occupiedTileCommands},
        {"duplicate-route invalidation and chase-to-ground intent", groundRouteInvalidation},
        {"click target chase, timed attacks, cancellation, and death", targetedCombat},
        {"stable target IDs and equipment attack speed", targetIdentityAndAttackSpeed},
        {"moving targets and wall-occluded melee", targetRepathAndOcclusion},
        {"attack cadence across all 27 equipment combinations", attackCadence},
        {"smooth multi-waypoint movement", navigation}, {"paper-doll equipment", wardrobe},
        {"arc vs bounding-box geometry", attackGeometry}, {"player timers and mitigation", playerTimersAndArmor},
        {"one hit per swing and enemy removal", oncePerSwingAndDeath}, {"AI windup and dodging", enemyAiAndDodge},
        {"death, full sanctum playthrough, and restart", deathAndFullRun},
        {"level connectivity and 100-second stress run", levelAndLongRun}
    };
    int failures = 0;
    for (const auto& test : tests) {
        try { test.second(); std::cout << "[PASS] " << test.first << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "[FAIL] " << test.first << ": " << e.what() << '\n'; }
    }
    std::cout << tests.size() << " suites, " << assertions << " assertions, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}

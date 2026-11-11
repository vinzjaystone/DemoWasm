#include "world.h"
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
    nav.assign(finder.find(grid, {1, 1}, {6, 6}), p);
    p = nav.advance(grid, p, 20, 1, Player::Radius);
    CHECK(!nav.moving());
    CHECK(near(p.x, 6.5f));
    CHECK(near(p.y, 6.5f));
    p = {2.7f, 2.5f};
    nav.assign(finder.find(grid, {2, 2}, {2, 6}), p);
    CHECK(near(nav.waypoints.front().x, 2.5f));
    for (int i = 0; i < 300; ++i) p = nav.advance(grid, p, 3.0f, 1.0f / 120.0f, Player::Radius);
    CHECK(!nav.moving());
    CHECK(near(p.y, 6.5f));
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
    for (int i = 0; i < 60; ++i) player.update(Grid(20, 20), 1.0f / 120.0f);
    CHECK(!player.attack.active);
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
    // Finish with repeated aim-correct attacks; each swing can only hit once.
    for (int i = 0; i < 500 && !world.cleared(); ++i) {
        if (!world.enemies.empty()) world.attackAt(world.enemies.front().position);
        world.update(1.0f / 120.0f);
        if (!world.enemies.empty() && distance(world.player.position, world.enemies.front().position) > 1.4f)
            world.movePlayerTo(world.enemies.front().position);
    }
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
    float repath = 0;
    for (int frame = 0; frame < 36000 && run.player.alive() && !run.cleared(); ++frame) {
        const Enemy* target = nullptr;
        float closest = 1e9f;
        for (const Enemy& enemy : run.enemies) {
            const float d = distance(run.player.position,enemy.position);
            if (enemy.alive() && d<closest) { closest=d; target=&enemy; }
        }
        CHECK(target!=nullptr);
        if (repath<=0 && closest>1.25f) {
            run.movePlayerTo(target->position);
            repath=0.35f;
        }
        run.attackAt(target->position);
        if (run.player.health<65 && run.player.potions>0) run.heal();
        run.update(1.0f/120.0f);
        repath-=1.0f/120.0f;
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
        {"projection and negative picking", projection}, {"grid collision and tunneling", gridAndCollision},
        {"A* basic cases", pathBasics}, {"A* maze and no corner cutting", pathCornersAndMaze},
        {"A* optimality vs Dijkstra on 80 maps", optimalPaths},
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

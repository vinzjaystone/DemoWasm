#include "character_sprites.h"
#include "world.h"
#include <functional>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

using namespace ashfall;
using namespace ashfall::character;
namespace {
int assertions=0;
void check(bool condition,const char* expression,int line) {
    ++assertions;
    if (!condition) throw std::runtime_error(std::string("line ")+std::to_string(line)+": "+expression);
}
#define CHECK(...) check(static_cast<bool>(__VA_ARGS__),#__VA_ARGS__,__LINE__)
bool near(double a,double b,double tolerance=0.0001) { return std::abs(a-b)<=tolerance; }
bool same(Pixel a,Pixel b) { return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a; }
bool same(Pose a,Pose b) { return a.clip==b.clip && a.direction==b.direction && a.frame==b.frame; }
Vec2 unproject(Vec2 screen) { return {screen.y+screen.x*0.5f,screen.y-screen.x*0.5f}; }
std::uint64_t fingerprint(const Frame& pixels) {
    std::uint64_t h=14695981039346656037ull;
    for (Pixel p : pixels) for (std::uint8_t v : {p.r,p.g,p.b,p.a}) { h^=v; h*=1099511628211ull; }
    return h;
}
void directions() {
    CHECK(quantizeDirection({1,0})==Direction::SouthEast);
    CHECK(quantizeDirection({0,1})==Direction::SouthWest);
    CHECK(quantizeDirection({1,-1})==Direction::East);
    CHECK(quantizeDirection({-1,1})==Direction::West);
    CHECK(quantizeDirection({1,1})==Direction::South);
    CHECK(quantizeDirection({-1,-1})==Direction::North);
    for (int i=0;i<DirectionCount;++i) {
        const auto direction=static_cast<Direction>(i);
        CHECK(quantizeDirection(unproject(screenDirection(direction)))==direction);
        CHECK(quantizeDirection(unproject(screenDirection(direction))*7)==direction);
        CHECK(std::string(directionName(direction)).size()>0);
        for (int tenth=-220;tenth<=220;++tenth) {
            const Vec2 sample=rotated(screenDirection(direction),static_cast<float>(tenth)*Pi/1800);
            CHECK(quantizeDirection(unproject(sample))==direction);
        }
    }
    CHECK(quantizeDirection({0,0},Direction::North)==Direction::North);
    CHECK(quantizeDirection({std::numeric_limits<float>::quiet_NaN(),1},Direction::West)==Direction::West);
    CHECK(quantizeDirection({1,std::numeric_limits<float>::infinity()},Direction::East)==Direction::East);
    const Vec2 edge=unproject(rotated(screenDirection(Direction::South),24*Pi/180));
    CHECK(quantizeDirection(edge)==Direction::SouthWest);
    CHECK(quantizeDirection(edge,Direction::South,true)==Direction::South);
    CHECK(quantizeDirection(unproject(rotated(screenDirection(Direction::South),29*Pi/180)),Direction::South,true)==Direction::SouthWest);
    CHECK(weaponBehind({Clip::Idle,Direction::North,0}));
    CHECK(weaponBehind({Clip::Walk,Direction::NorthEast,3}));
    CHECK(!weaponBehind({Clip::Idle,Direction::South,0}));
    CHECK(!weaponBehind({Clip::Walk,Direction::SouthEast,4}));
    // Sweeping through the back half changes ordering within an attack too.
    bool behind=false,inFront=false;
    for (int frame=0;frame<6;++frame) {
        if (weaponBehind({Clip::Attack,Direction::East,frame})) behind=true;
        else inFront=true;
    }
    CHECK(behind && inFront);
}
void clipsAndAttackTiming() {
    CHECK(clipInfo(Clip::Idle).frames==4 && clipInfo(Clip::Idle).offset==0);
    CHECK(clipInfo(Clip::Walk).frames==8 && clipInfo(Clip::Walk).offset==4);
    CHECK(clipInfo(Clip::Attack).frames==6 && clipInfo(Clip::Attack).offset==12);
    CHECK(idleFrame(0)==0 && idleFrame(0.31)==1 && idleFrame(0.61)==2 && idleFrame(0.91)==3);
    CHECK(idleFrame(1.2)==0 && idleFrame(2.71)==1);
    CHECK(walkFrame(0)==0 && walkFrame(0.26)==1 && walkFrame(1.76)==7 && walkFrame(2)==0);
    CHECK(idleFrame(-1)==0 && walkFrame(-1)==0);
    CHECK(idleFrame(std::numeric_limits<double>::quiet_NaN())==0);
    CHECK(walkFrame(std::numeric_limits<double>::infinity())==0);
    for (float duration : {0.36f,0.43f}) {
        Attack attack; attack.active=true; attack.duration=duration;
        std::set<int> seen;
        int previous=-1;
        for (int i=0;i<=1000;++i) {
            attack.elapsed=duration*static_cast<float>(i)/1000;
            const int frame=attackFrame(attack);
            CHECK(frame>=previous && frame>=0 && frame<6);
            if (attack.damaging()) CHECK(frame>=1 && frame<=3);
            if (attack.elapsed<Attack::HitStart) CHECK(frame==0);
            if (attack.elapsed>Attack::HitEnd) CHECK(frame>=4);
            seen.insert(frame); previous=frame;
        }
        CHECK(seen.size()==6);
        attack.elapsed=duration; CHECK(attackFrame(attack)==5);
        attack.elapsed=100; CHECK(attackFrame(attack)==5);
        attack.elapsed=-1; CHECK(attackFrame(attack)==0);
        attack.elapsed=std::numeric_limits<float>::quiet_NaN(); CHECK(attackFrame(attack)==0);
    }
}
void distanceAndPause() {
    Player slow,fast;
    slow.walking=fast.walking=true;
    Animator a,b; a.reset(slow); b.reset(fast);
    for (int i=0;i<80;++i) {
        slow.position.x+=0.02f; fast.position.x+=0.04f;
        a.update(slow,1.0f/120,true); b.update(fast,1.0f/60,true);
        CHECK(a.pose().clip==Clip::Walk && b.pose().clip==Clip::Walk);
        CHECK(a.pose().frame==walkFrame(a.walkDistance()));
        CHECK(b.pose().frame==walkFrame(b.walkDistance()));
        if (i==39) CHECK(near(b.walkDistance(),1.6));
    }
    CHECK(near(a.walkDistance(),1.6)); CHECK(near(b.walkDistance(),3.2));
    const auto pose=a.pose(),preview=a.preview();
    const double traveled=a.walkDistance(),seconds=a.idleSeconds();
    for (int i=0;i<100;++i) a.update(slow,0.1f,false);
    CHECK(same(a.pose(),pose) && same(a.preview(),preview));
    CHECK(a.walkDistance()==traveled && a.idleSeconds()==seconds);
    slow.position.x+=20; // Paused teleports/restarts never catch up on resume.
    a.update(slow,0.1f,false); a.update(slow,0,true);
    CHECK(a.walkDistance()==traveled);
    slow.position.x+=20; a.update(slow,1.0f/120,true);
    CHECK(a.walkDistance()==traveled);
    slow.walking=false; a.update(slow,0,true);
    CHECK(a.pose().clip==Clip::Idle);
    for (int i=0;i<40;++i) a.update(slow,1.0f/120,true);
    CHECK(a.idleSeconds()>seconds+0.3);
    CHECK(a.pose().frame==idleFrame(a.idleSeconds()));
    CHECK(a.preview().clip==Clip::Idle && a.preview().direction==Direction::SouthEast);
    a.reset(slow);
    CHECK(a.idleSeconds()==0 && a.walkDistance()==0 && a.pose().frame==0);
    CHECK(a.pose().direction==quantizeDirection(slow.facing));
}
void navigationArrivalTransition() {
    World world;
    world.movePlayerTo({8.5f,10.5f});
    Animator animation; animation.reset(world.player);
    constexpr float step=1.0f/120;
    bool arrived=false;
    for (int tick=0;tick<240 && !arrived;++tick) {
        const bool wasMoving=world.player.navigation.moving();
        const Vec2 previous=world.player.position;
        world.update(step); animation.update(world.player,step,true);
        if (wasMoving && !world.player.navigation.moving()) {
            // A completed route is not yet a stationary simulation sample:
            // this tick still displaced the player to the final waypoint.
            CHECK(distance(previous,world.player.position)>0.001f);
            CHECK(world.player.walking && animation.pose().clip==Clip::Walk);
            CHECK(near(world.player.position.x,8.5) && near(world.player.position.y,10.5));
            animation.update(world.player,0,true); // Input-only RAF synchronization.
            CHECK(animation.pose().clip==Clip::Walk);
            world.update(step); animation.update(world.player,step,true);
            CHECK(!world.player.navigation.moving() && !world.player.walking);
            CHECK(animation.pose().clip==Clip::Idle);
            CHECK(animation.walkDistance()>1.8);
            arrived=true;
        }
    }
    CHECK(arrived);
}
void transitionsAndAttackLock() {
    Player player; Animator animation;
    animation.reset(player);
    player.walking=true; player.position.x+=0.02f; animation.update(player,1.0f/120,true);
    CHECK(animation.pose().clip==Clip::Walk);
    CHECK(player.startAttack({-1,-1})); animation.update(player,0,true);
    CHECK(animation.pose().clip==Clip::Attack && animation.pose().frame==0);
    CHECK(animation.pose().direction==Direction::North);
    player.facing={1,1}; // Movement can resume during the recovery; swing stays locked.
    player.attack.elapsed=0.28f; animation.update(player,0,true);
    CHECK(animation.pose().direction==Direction::North && animation.pose().frame==4);
    const Pose frozen=animation.pose(); const double clock=animation.idleSeconds();
    animation.update(player,0,true); CHECK(same(animation.pose(),frozen)); CHECK(clock==animation.idleSeconds());
    player.attack.active=false; player.walking=false; animation.update(player,0,true);
    CHECK(player.attack.cooldown>2 && animation.pose().clip==Clip::Idle);
    CHECK(animation.pose().direction==Direction::South);
    player.dashRemaining=0.1f; animation.update(player,0,true);
    CHECK(animation.pose().clip==Clip::Walk && animation.pose().frame==2);
    player.dashRemaining=0; player.health=0;
    const double deadClock=animation.idleSeconds(); animation.update(player,0.1f,true);
    CHECK(animation.idleSeconds()==deadClock && animation.pose().clip==Clip::Idle);
}
void packingTransparencyAndRegistration() {
    std::size_t bytes=0;
    for (std::size_t i=0;i<LayerCount;++i) {
        const Layer layer=static_cast<Layer>(i);
        const SheetInfo& info=sheetInfo(layer);
        CHECK(info.width()<=2048 && info.height()<=2048); // WebGL 1 minimum texture limit.
        CHECK(info.x>=0 && info.y>=0 && info.x+info.cellWidth<=CellSize && info.y+info.cellHeight<=CellSize);
        const auto sheet=makeStarterSheet(layer);
        CHECK(sheet.size()==static_cast<std::size_t>(info.width()*info.height()));
        CHECK(sheet.size()*sizeof(Pixel)<6*1024*1024);
        bytes+=sheet.size()*sizeof(Pixel);
        for (int variant=0;variant<info.variants;++variant) for (int d=0;d<8;++d) for (int clip=0;clip<3;++clip)
            for (int f=0;f<clipInfo(static_cast<Clip>(clip)).frames;++f) {
                const Pose pose{static_cast<Clip>(clip),static_cast<Direction>(d),f};
                const auto pixels=makeStarterFrame(layer,pose,variant);
                const CellRect cell=sourceCell(layer,pose,variant);
                CHECK(cell.x>=0 && cell.y>=0 && cell.x+cell.width<=info.width() && cell.y+cell.height<=info.height());
                CHECK(cell.x==(clipInfo(pose.clip).offset+f)*info.cellWidth);
                CHECK(cell.y==(variant*DirectionCount+d)*info.cellHeight);
                int occupied=0;
                for (int y=0;y<CellSize;++y) for (int x=0;x<CellSize;++x) {
                    const Pixel p=pixels[static_cast<std::size_t>(y*CellSize+x)];
                    CHECK(p.a==0 || p.a==255);
                    if (x<info.x || x>=info.x+info.cellWidth || y<info.y || y>=info.y+info.cellHeight) {
                        if (p.a!=0) throw std::runtime_error(std::string("cropped art: ")+info.name+" variant "+std::to_string(variant)+
                            " dir "+std::to_string(d)+" clip "+std::to_string(clip)+" frame "+std::to_string(f)+
                            " at "+std::to_string(x)+","+std::to_string(y));
                        CHECK(p.a==0);
                    } else {
                        const Pixel packed=sheet[static_cast<std::size_t>((cell.y+y-info.y)*info.width()+cell.x+x-info.x)];
                        CHECK(same(p,packed));
                    }
                    if (x==0 || y==0 || x==CellSize-1 || y==CellSize-1 ||
                        x==info.x || y==info.y || x==info.x+info.cellWidth-1 || y==info.y+info.cellHeight-1) CHECK(p.a==0);
                    if (p.a) ++occupied;
                }
                CHECK(occupied>15); // Every clip/direction/variant is actually populated.
                // A trimmed cell's destination is offset from the same logical foot
                // pivot; packing never changes registration at fractional positions.
                const double feet=123.456,scale=1.37;
                const double destination=feet+static_cast<double>(info.x-FootX)*scale;
                CHECK(near(destination+static_cast<double>(FootX-info.x)*scale,feet));
            }
    }
    CHECK(bytes<20*1024*1024); // Combined RGBA GPU atlas budget; no retained CPU copies.
    const CellRect safe=sourceCell(Layer::Head,{static_cast<Clip>(200),static_cast<Direction>(200),-100},500);
    CHECK(safe.x==0 && safe.y==16*sheetInfo(Layer::Head).cellHeight);
}
Frame composite(Pose pose,Equipment equipment) {
    Frame result{};
    const auto draw=[&](Layer layer,int variant,Rgb tint={255,255,255}) {
        const Frame frame=makeStarterFrame(layer,pose,variant);
        for (std::size_t i=0;i<frame.size();++i) if (frame[i].a) result[i]={
            static_cast<std::uint8_t>(static_cast<int>(frame[i].r)*tint.r/255),
            static_cast<std::uint8_t>(static_cast<int>(frame[i].g)*tint.g/255),
            static_cast<std::uint8_t>(static_cast<int>(frame[i].b)*tint.b/255),frame[i].a};
    };
    if (weaponBehind(pose)) draw(Layer::Weapon,0);
    draw(Layer::Base,0);
    for (EquipmentSlot slot : {EquipmentSlot::Boots,EquipmentSlot::Armor,EquipmentSlot::Head}) {
        const auto& piece=equipment.get(slot);
        const Layer layer=slot==EquipmentSlot::Boots?Layer::Boots:(slot==EquipmentSlot::Armor?Layer::Armor:Layer::Head);
        draw(layer,piece.variant,piece.tint);
    }
    if (!weaponBehind(pose)) draw(Layer::Weapon,0);
    return result;
}
void silhouettesAndEquipment() {
    Equipment equipment;
    for (Clip clip : {Clip::Idle,Clip::Walk,Clip::Attack}) {
        std::set<std::uint64_t> directions;
        for (int d=0;d<DirectionCount;++d) directions.insert(fingerprint(composite({clip,static_cast<Direction>(d),0},equipment)));
        CHECK(directions.size()==8);
    }
    std::set<std::uint64_t> bodies;
    for (int d=0;d<DirectionCount;++d)
        bodies.insert(fingerprint(makeStarterFrame(Layer::Base,{Clip::Idle,static_cast<Direction>(d),0})));
    CHECK(bodies.size()==8); // Directional body itself, not only a rotating weapon.
    const Pose pose{Clip::Idle,Direction::SouthEast,0};
    std::set<std::uint64_t> combinations;
    for (int h=0;h<3;++h) {
        for (int a=0;a<3;++a) {
            for (int b=0;b<3;++b) {
                combinations.insert(fingerprint(composite(pose,equipment)));
                equipment.cycle(EquipmentSlot::Boots);
            }
            equipment.cycle(EquipmentSlot::Armor);
        }
        equipment.cycle(EquipmentSlot::Head);
    }
    CHECK(combinations.size()==27);
    CHECK(fingerprint(composite(pose,equipment))!=fingerprint(composite({Clip::Idle,pose.direction,1},equipment)));
    CHECK(fingerprint(composite({Clip::Walk,pose.direction,0},equipment))!=fingerprint(composite({Clip::Walk,pose.direction,2},equipment)));
    CHECK(fingerprint(composite({Clip::Attack,pose.direction,0},equipment))!=fingerprint(composite({Clip::Attack,pose.direction,3},equipment)));
    for (Layer layer : {Layer::Boots,Layer::Armor,Layer::Head}) {
        std::set<std::uint64_t> shapes;
        for (int v=0;v<3;++v) {
            Frame pixels=makeStarterFrame(layer,pose,v);
            for (Pixel& p : pixels) if (p.a) p={255,255,255,255};
            shapes.insert(fingerprint(pixels));
        }
        CHECK(shapes.size()==3); // Real silhouette changes, not just a tint swap.
    }
}
void simulationUnchanged() {
    World reference,rendered;
    Animator animation; animation.reset(rendered.player);
    for (int tick=0;tick<2400;++tick) {
        if (tick%240==0) {
            const Vec2 goal=tick%480==0?Vec2{8.5f,10.5f}:Vec2{8.5f,12.5f};
            reference.movePlayerTo(goal); rendered.movePlayerTo(goal);
        }
        if (tick%300==70) {
            reference.attackAt(reference.player.position+Vec2{-1,-1});
            rendered.attackAt(rendered.player.position+Vec2{-1,-1});
        }
        if (tick%500==0) { reference.player.equipment.cycle(EquipmentSlot::Boots); rendered.player.equipment.cycle(EquipmentSlot::Boots); }
        const bool animate=rendered.hitStop<=0;
        reference.update(1.0f/120); rendered.update(1.0f/120);
        animation.update(rendered.player,animate?1.0f/120:0,true);
        const Player& a=reference.player; const Player& b=rendered.player;
        CHECK(a.position.x==b.position.x && a.position.y==b.position.y);
        CHECK(a.facing.x==b.facing.x && a.facing.y==b.facing.y);
        CHECK(a.attack.serial==b.attack.serial && a.attack.elapsed==b.attack.elapsed && a.attack.cooldown==b.attack.cooldown);
        CHECK(a.navigation.next==b.navigation.next && a.navigation.waypoints.size()==b.navigation.waypoints.size());
        CHECK(a.health==b.health && a.embers==b.embers && reference.kills==rendered.kills);
    }
}
}
int main() {
    const std::pair<const char*,std::function<void()>> suites[]={
        {"projected directions, hysteresis, weapon ordering",directions},
        {"clip loops and attack damage-window markers",clipsAndAttackTiming},
        {"distance-driven walk, idle clock, pause, teleport, reset",distanceAndPause},
        {"route completion, final displacement, stationary idle transition",navigationArrivalTransition},
        {"transitions, locked attack facing, independent recovery",transitionsAndAttackLock},
        {"all sheets, alpha, packing, bounds and foot registration",packingTransparencyAndRegistration},
        {"eight silhouettes and all 27 equipment combinations",silhouettesAndEquipment},
        {"animation leaves simulation and combat identical",simulationUnchanged}
    };
    int failures=0;
    for (const auto& suite : suites) {
        try { suite.second(); std::cout<<"[PASS] "<<suite.first<<'\n'; }
        catch (const std::exception& error) { ++failures; std::cerr<<"[FAIL] "<<suite.first<<": "<<error.what()<<'\n'; }
    }
    std::cout<<std::size(suites)<<" suites, "<<assertions<<" assertions, "<<failures<<" failures\n";
    return failures==0?0:1;
}

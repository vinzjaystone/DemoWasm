#include "run_save.h"
#include "editor_recovery.h"
#include "local_store.h"
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ashfall;
using namespace ashfall::save;
namespace {
int assertions=0;
void check(bool value,const char* expression,int line) {
    ++assertions; if (!value) throw std::runtime_error(std::string("line ")+std::to_string(line)+": "+expression);
}
#define CHECK(...) check(static_cast<bool>(__VA_ARGS__),#__VA_ARGS__,__LINE__)
snapshot::Bytes run(const World& world) {
    std::string error; auto bytes=encodeRun(world,error); CHECK(!bytes.empty() && error.empty()); return bytes;
}
World clone(const World& world) {
    const auto bytes=run(world); std::string error;
    auto loaded=decodeRun(bytes.data(),bytes.size(),error); CHECK(loaded && error.empty());
    CHECK(run(*loaded)==bytes); return std::move(*loaded);
}
void put32(snapshot::Bytes& bytes,std::size_t offset,std::uint32_t value) {
    CHECK(offset+4<=bytes.size());
    for (unsigned i=0;i<4;++i) bytes[offset+i]=static_cast<unsigned char>((value>>(8u*i))&255);
}
void fix(snapshot::Bytes& bytes) {
    put32(bytes,12,static_cast<std::uint32_t>(bytes.size()-snapshot::HeaderBytes));
    put32(bytes,16,snapshot::checksum(bytes.data()+snapshot::HeaderBytes,bytes.size()-snapshot::HeaderBytes));
}
void runRoundTrips() {
    World world;
    for (auto slot:{EquipmentSlot::Head,EquipmentSlot::Armor,EquipmentSlot::Boots}) world.cycleEquipment(slot);
    world.player.health=73; world.player.potions=1; world.player.embers=207; world.player.experience=140;
    world.movePlayerTo({11.5f,12.5f}); world.update(0.075f);
    auto loaded=clone(world);
    CHECK(loaded.player.navigation.moving() && loaded.player.health==73 && loaded.player.experience==140);
    CHECK(loaded.player.equipment.armor()==world.player.equipment.armor());
    CHECK(loaded.particles.empty() && loaded.damageNumbers.empty() && loaded.notice.empty());
    for (int i=0;i<80;++i) { world.update(1.0f/120); loaded.update(1.0f/120); CHECK(run(world)==run(loaded)); }
    // Timers are frozen by not updating; saving/loading has no implicit tick.
    const auto bytes=run(loaded); CHECK(run(clone(loaded))==bytes);
    World dead; dead.player.takeDamage(10000,{});
    CHECK(!clone(dead).player.alive());
    World won; won.kills=won.totalEnemies; won.enemies.clear(); won.elapsed=317.5f;
    CHECK(clone(won).cleared());
    CHECK(clone(won).elapsed==317.5f);
}
void inFlightCombat() {
    World world;
    world.player.position={5.5f,4.5f};
    CHECK(world.selectEnemy(world.enemies.front().id));
    for (int i=0;i<14;++i) world.update(1.0f/120);
    CHECK(world.player.attack.active && world.player.attack.cooldown>2.0f);
    const int health=world.enemies.front().health;
    CHECK(health<world.enemies.front().maxHealth);
    auto loaded=clone(world);
    CHECK(loaded.selectedEnemyId()==world.selectedEnemyId());
    CHECK(loaded.enemies.front().lastPlayerAttack==world.player.attack.serial);
    for (int i=0;i<60;++i) {
        world.update(1.0f/120); loaded.update(1.0f/120);
        CHECK(run(world)==run(loaded));
    }
    CHECK(loaded.enemies.front().health==health); // Never deal the saved swing twice.
    World hostile; hostile.player.position={5.5f,4.5f};
    for (int i=0;i<24;++i) hostile.update(1.0f/120);
    CHECK(hostile.enemies.front().state==EnemyState::Attack && !hostile.enemies.front().attackApplied);
    auto windup=clone(hostile);
    for (int i=0;i<36;++i) { hostile.update(1.0f/120); windup.update(1.0f/120); CHECK(run(hostile)==run(windup)); }
    CHECK(hostile.enemies.front().attackApplied && hostile.player.health<Player::MaxHealth);
    auto afterHit=clone(hostile); const int hitHealth=hostile.player.health;
    hostile.player.invulnerable=afterHit.player.invulnerable=0;
    hostile.update(0.05f); afterHit.update(0.05f);
    CHECK(afterHit.player.health==hitHealth && run(hostile)==run(afterHit)); // Enemy hits do not replay either.
    World dash;
    CHECK(dash.dashTo(dash.player.position+Vec2{1,0})); dash.update(0.05f);
    dash.player.facing={-1,0}; // Facing is not the private dash velocity.
    auto resumed=clone(dash);
    for (int i=0;i<30;++i) { dash.update(1.0f/120); resumed.update(1.0f/120); CHECK(run(dash)==run(resumed)); }
    CHECK(resumed.player.position.x>8.5f);
    World chase; CHECK(chase.selectEnemy(chase.enemies.front().id)); chase.update(0.1f);
    auto restored=clone(chase); CHECK(restored.player.navigation.moving());
    for (int i=0;i<120;++i) { chase.update(1.0f/120); restored.update(1.0f/120); CHECK(run(chase)==run(restored)); }
}
void repeatedCheckpoints() {
    World world;
    for (int i=0;i<6000;++i) {
        if (i%211==0) world.movePlayerTo({static_cast<float>(2+(i/211)%15)+0.5f,static_cast<float>(2+(i/389)%15)+0.5f});
        if (i%307==0 && world.livingEnemies()>0) {
            for (const auto& enemy:world.enemies) if (enemy.alive()) { world.selectEnemy(enemy.id); break; }
        }
        if (i%173==0) world.attackAt(world.player.position+world.player.facing);
        if (i%487==0) world.dashTo(world.player.position+Vec2{1,-1});
        if (i%149==0) world.heal();
        if (i%191==0) world.cycleEquipment(EquipmentSlot::Boots);
        world.update(1.0f/120);
        if (i%37==0) world=clone(world);
    }
    CHECK(std::isfinite(world.elapsed));
}
void hostileRuns() {
    const auto original=run(World{}); std::string error;
    CHECK(!decodeRun(nullptr,0,error) && !error.empty());
    CHECK(!decodeRun(original.data(),MaxRunBytes+1,error));
    for (std::size_t n=0;n<original.size();++n) CHECK(!decodeRun(original.data(),n,error));
    for (std::size_t i=0;i<original.size();++i) {
        auto bad=original; bad[i]^=128; CHECK(!decodeRun(bad.data(),bad.size(),error));
    }
    for (auto mutation:{std::pair<std::size_t,std::uint32_t>{20,2}, {24,0}, {28,0x7fc00000u}, {32,0x7f800000u},
                        {36,99}, {40,0}, {44,0}, {52,999}, {56,99}, {64,99}, {72,0x7f800000u},
                        {104,0xffffffffu}, {108,1}, {115,121}, {119,4}, {185,0xffffffffu},
                        {189,0}, {193,8}, {197,9}, {253,151}, {278,1}}) {
        auto bad=original; put32(bad,mutation.first,mutation.second); fix(bad);
        CHECK(!decodeRun(bad.data(),bad.size(),error));
    }
    auto extra=original; extra.push_back(0); fix(extra); CHECK(!decodeRun(extra.data(),extra.size(),error));
    World invalid; invalid.player.health=-1; CHECK(encodeRun(invalid,error).empty());
    invalid=World{}; invalid.grid.set({8,12},TileKind::Column); CHECK(encodeRun(invalid,error).empty());
    invalid=World{}; invalid.enemies.push_back(invalid.enemies.front()); CHECK(encodeRun(invalid,error).empty());
    invalid=World{}; invalid.enemies.front().health=0; CHECK(encodeRun(invalid,error).empty());
    invalid=World{}; invalid.player.navigation.waypoints.resize(401); CHECK(encodeRun(invalid,error).empty());
    // Valid-checksum mutations exercise bounded parsers beyond the envelope.
    for (std::uint32_t i=1;i<1500;++i) {
        auto bad=original;
        const std::size_t offset=20+hash(i)%(bad.size()-20);
        bad[offset]^=static_cast<unsigned char>(1u<<(i%8u)); fix(bad);
        if (auto accepted=decodeRun(bad.data(),bad.size(),error)) CHECK(!run(*accepted).empty());
    }
}
void recoveryRoundTrips() {
    Draft draft; draft.name="sprite-v2"; draft.image=sprite::Bitmap{19,23};
    draft.image.pixels.front()={4,7,231,119}; draft.image.pixels.back()={91,1,23,0};
    draft.foreground={255,60,88,123}; draft.grid=false; draft.mirror=true;
    std::string error;
    const auto bytes=encodeDraft(draft,error); CHECK(!bytes.empty() && error.empty());
    auto loaded=decodeDraft(bytes.data(),bytes.size(),error);
    CHECK(loaded && loaded->image==draft.image && loaded->name==draft.name && loaded->mirror && !loaded->grid);
    CHECK(encodeDraft(*loaded,error)==bytes);
    sprite::Document document;
    CHECK(document.recover(loaded->image) && document.dirty() && document.undoDepth()==0 && document.redoDepth()==0);
    document.paintLine({1,0},{2,0},{1,2,3,4}); document.commitStroke(); CHECK(document.undo() && document.dirty());
    document.markSaved(); CHECK(!document.dirty());
    auto badImage=draft.image; badImage.pixels.clear(); CHECK(!document.recover(badImage) && !document.dirty());
    art::Workspace workspace;
    const auto key=workspace.key();
    workspace.document().paintLine({0,0},{95,95},{250,128,63,119});
    CHECK(workspace.document().editing() && workspace.document().undoDepth()==0);
    Draft gear; gear.equipment=true; gear.name=workspace.project().name; gear.key=key;
    gear.project=workspace.project(); gear.project.set(key,art::pixels(workspace.document().bitmap()));
    auto second=key; second.slot=EquipmentSlot::Boots; second.variant=2; second.pose={character::Clip::Attack,character::Direction::North,5};
    gear.project.set(second,character::Frame{}); gear.key=second;
    const auto encoded=encodeDraft(gear,error); auto decoded=decodeDraft(encoded.data(),encoded.size(),error);
    CHECK(decoded && decoded->project==gear.project && decoded->key==second);
    CHECK(workspace.document().editing() && workspace.document().undoDepth()==0); // Autosave is read-only.
    workspace.document().commitStroke(); CHECK(workspace.document().undoDepth()==1);
    art::Workspace recovered; CHECK(recovered.recover(decoded->project,decoded->key));
    CHECK(recovered.key()==second && recovered.dirty() && recovered.document().undoDepth()==0);
    recovered.document().markSaved(); CHECK(recovered.dirty()); // A frame PNG is not a gear save.
    recovered.markSaved(); CHECK(!recovered.dirty());
    CHECK(recovered.select(key)); CHECK(recovered.document().bitmap().at({95,95})==sprite::Pixel{250,128,63,119});
    auto invalid=key; invalid.variant=8;
    CHECK(!recovered.recover(decoded->project,invalid) && !recovered.dirty());
    Draft unchanged; unchanged.equipment=true; unchanged.name=unchanged.project.name;
    auto empty=encodeDraft(unchanged,error); auto starter=decodeDraft(empty.data(),empty.size(),error);
    CHECK(starter && recovered.recover(starter->project,starter->key) && recovered.dirty());
}
void hostileDrafts() {
    Draft draft; std::string error;
    const auto original=encodeDraft(draft,error); CHECK(!original.empty());
    CHECK(!decodeDraft(nullptr,0,error)); CHECK(!decodeDraft(original.data(),MaxDraftBytes+1,error));
    for (std::size_t n=0;n<original.size();++n) CHECK(!decodeDraft(original.data(),n,error));
    for (std::size_t i=0;i<original.size();i+=13) { auto bad=original; bad[i]^=1; CHECK(!decodeDraft(bad.data(),bad.size(),error)); }
    auto bad=original; bad[20]=2; fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    bad=original; bad[21]=255; fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    bad=original; bad[22]='/'; fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    const std::size_t widthOffset=22+draft.name.size()+8;
    bad=original; put32(bad,widthOffset,0xffffffffu); fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    bad=original; bad.push_back(0); fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    draft.name="../file"; CHECK(encodeDraft(draft,error).empty());
    draft=Draft{}; draft.equipment=true; draft.name=draft.project.name;
    const auto gear=encodeDraft(draft,error); const std::size_t key=22+draft.name.size()+8;
    bad=gear; bad[key]=8; fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    bad=gear; put32(bad,key+5,0xffffffffu); fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error));
    bad=gear; bad.back()^=1; fix(bad); CHECK(!decodeDraft(bad.data(),bad.size(),error)); // Nested project checksum/metadata.
    draft.project.name="different"; CHECK(encodeDraft(draft,error).empty());
}
void scheduling() {
    AutosaveClock clock;
    clock.advance(10); CHECK(!clock.due()); clock.changed();
    clock.advance(0.599f); CHECK(!clock.due()); clock.advance(0.002f); CHECK(clock.due());
    clock.sent(); CHECK(!clock.pending() && !clock.due());
    for (int i=0;i<210;++i) { clock.changed(); clock.advance(0.01f); }
    CHECK(clock.due()); clock.sent();
    clock.changed(); clock.advance(std::numeric_limits<float>::infinity()); clock.advance(-1); CHECK(!clock.due());
}
void nativeStorage() {
    const auto root=std::filesystem::path{"build"}/("save-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code code; std::filesystem::remove_all(path,code); } } cleanup{root};
    LocalStore store{root}; std::string error;
    auto missing=store.read(Slot::Run,MaxRunBytes); CHECK(!missing.found && missing.error.empty() && !std::filesystem::exists(root));
    const auto bytes=run(World{});
    CHECK(store.write(Slot::Run,bytes,error) && error.empty());
    auto read=store.read(Slot::Run,MaxRunBytes); CHECK(read.found && read.error.empty() && read.bytes==bytes);
    CHECK(!store.write(Slot::Run,{},error)); CHECK(store.read(Slot::Run,MaxRunBytes).bytes==bytes);
    CHECK(!store.write(Slot::Run,snapshot::Bytes(MaxRunBytes+1),error)); CHECK(store.read(Slot::Run,MaxRunBytes).bytes==bytes);
    World changed; changed.player.health=29; const auto next=run(changed);
    CHECK(store.write(Slot::Run,next,error)); CHECK(store.read(Slot::Run,MaxRunBytes).bytes==next);
    CHECK(!store.read(Slot::Run,4).error.empty());
    CHECK(readFile(store.path(Slot::Run),MaxRunBytes).bytes==next);
    CHECK(!readFile(store.path(Slot::Run),4).error.empty());
    Draft draft; auto recovery=encodeDraft(draft,error);
    CHECK(store.write(Slot::Editor,recovery,error)); CHECK(store.read(Slot::Editor,MaxDraftBytes).bytes==recovery);
    CHECK(store.clear(Slot::Editor,error)); CHECK(!store.read(Slot::Editor,MaxDraftBytes).found);
    CHECK(store.read(Slot::Run,MaxRunBytes).bytes==next); // Independent slots.
    LocalStore obstructed{store.path(Slot::Run)}; CHECK(!obstructed.write(Slot::Run,bytes,error));
    CHECK(store.read(Slot::Run,MaxRunBytes).bytes==next);
    LocalStore replaceBlocked{root/"replace-blocked"};
    std::filesystem::create_directories(replaceBlocked.path(Slot::Run));
    const auto marker=replaceBlocked.path(Slot::Run)/"previous-data";
    { std::ofstream output(marker); output<<"keep me"; }
    CHECK(!replaceBlocked.write(Slot::Run,bytes,error) && std::filesystem::exists(marker));
    for (const auto& entry:std::filesystem::recursive_directory_iterator(root)) CHECK(entry.path().filename().string().find(".tmp-")==std::string::npos);
    CHECK(store.clear(Slot::Run,error)); CHECK(store.clear(Slot::Run,error));
}
}
int main() {
    int failures=0;
    for (const auto& test:std::initializer_list<std::pair<const char*,std::function<void()>>>{
            {"run round trips and terminal states",runRoundTrips},{"in-flight combat, dash and pursuit",inFlightCombat},
            {"repeated simulation checkpoints",repeatedCheckpoints},{"hostile run saves",hostileRuns},
            {"PNG/gear recovery and dirty semantics",recoveryRoundTrips},{"hostile editor drafts",hostileDrafts},
            {"autosave coalescing",scheduling},{"native bounded atomic storage",nativeStorage}}) {
        try { test.second(); std::cout<<"[PASS] "<<test.first<<'\n'; }
        catch (const std::exception& error) { ++failures; std::cerr<<"[FAIL] "<<test.first<<": "<<error.what()<<'\n'; }
    }
    std::cout<<assertions<<" assertions, "<<failures<<" failures\n"; return failures?1:0;
}

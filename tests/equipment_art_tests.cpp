#include "equipment_art.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ashfall;
using namespace ashfall::art;
using namespace ashfall::character;
namespace {
int assertions=0;
void check(bool value,const char* expression,int line) {
    ++assertions;
    if (!value) throw std::runtime_error(std::string("line ")+std::to_string(line)+": "+expression);
}
#define CHECK(...) check(static_cast<bool>(__VA_ARGS__),#__VA_ARGS__,__LINE__)
const sprite::Pixel Ink{231,214,202,147};
void paint(Workspace& workspace,sprite::Point point={0,0}) {
    workspace.document().paintLine(point,point,Ink); workspace.document().commitStroke(); workspace.sync();
}
void identities() {
    std::size_t count=0;
    Equipment equipment;
    const int armor=equipment.armor(); const float speed=equipment.speedBonus(),aspd=equipment.attackSpeedBonus();
    for (int slot=0;slot<3;++slot) for (int variant=0;variant<3;++variant) {
        CHECK(Equipment::piece(static_cast<EquipmentSlot>(slot),variant).variant==variant);
        for (int clip=0;clip<3;++clip) for (int direction=0;direction<8;++direction)
            for (int f=0;f<clipInfo(static_cast<Clip>(clip)).frames;++f) {
                Key key{static_cast<EquipmentSlot>(slot),variant,{static_cast<Clip>(clip),static_cast<Direction>(direction),f}};
                CHECK(key.valid()); ++count;
                CHECK(bitmap(starter(key))==bitmap(makeStarterFrame(layer(key.slot),key.pose,variant)));
                CHECK(pixels(bitmap(starter(key)))==starter(key));
            }
    }
    CHECK(count==MaxFrames && count==1296);
    Key key;
    key.variant=3; CHECK(!key.valid()); key.variant=-1; CHECK(!key.valid()); key.variant=0;
    key.slot=static_cast<EquipmentSlot>(255); CHECK(!key.valid()); key.slot=EquipmentSlot::Armor;
    key.pose.clip=static_cast<Clip>(255); CHECK(!key.valid()); key.pose.clip=Clip::Idle;
    key.pose.direction=static_cast<Direction>(255); CHECK(!key.valid()); key.pose.direction=Direction::South;
    key.pose.frame=4; CHECK(!key.valid()); key.pose.frame=-1; CHECK(!key.valid());
    CHECK(equipment.armor()==armor && equipment.speedBonus()==speed && equipment.attackSpeedBonus()==aspd);
    CHECK(!validName("") && !validName("../armor") && !validName("<script>"));
    CHECK(!validName(std::string(29,'a')) && validName("ashguard-v1_2"));
}
void projectRoundTrip() {
    Project project; project.name="ashguard-v2";
    Key first{},second{EquipmentSlot::Boots,2,{Clip::Attack,Direction::NorthWest,5}};
    Frame frame=starter(first);
    frame[0]={20,40,90,111}; frame.back()={240,234,210,255};
    CHECK(project.set(first,frame)); CHECK(!project.set(first,frame));
    // A no-crop, noise-heavy frame exercises raw fallback and alpha preservation.
    Frame noise{};
    for (std::size_t i=0;i<noise.size();++i) noise[i]={static_cast<std::uint8_t>(i%256),static_cast<std::uint8_t>((i/256)%256),54,static_cast<std::uint8_t>((i*17)%256)};
    CHECK(project.set(second,noise));
    const auto bytes=encode(project); CHECK(!bytes.empty() && bytes.size()<MaxFileBytes);
    CHECK(bytes[8]==1 && bytes[12]==96 && bytes[14]==48 && bytes[16]==76);
    std::string error;
    const auto loaded=decode(bytes.data(),bytes.size(),error);
    CHECK(loaded && error.empty() && *loaded==project);
    CHECK(loaded->frame(first)[0]==frame[0] && loaded->frame(first).back()==frame.back());
    CHECK(loaded->frame(second)==noise);
    CHECK(encode(*loaded)==bytes); // Deterministic order and encoding, no native struct dumps.
    Project copy=project; CHECK(copy==project);
    CHECK(copy.set(first,starter(first))); CHECK(copy!=project && copy.frames().size()==1);
    CHECK(project.frames().size()==2 && project.frame(first)==frame); // Immutable copy-on-write.
    CHECK(!copy.set(first,starter(first)));
    Key invalid=first; invalid.variant=9; CHECK(!copy.set(invalid,frame));
    Project empty; auto emptyBytes=encode(empty); CHECK(emptyBytes.size()==37+empty.name.size());
    CHECK(decode(emptyBytes.data(),emptyBytes.size(),error)==std::optional<Project>{empty});
    empty.name="../../bad"; CHECK(encode(empty).empty());
}
void workspaceTransactions() {
    Workspace workspace; const Key first=workspace.key();
    CHECK(!workspace.dirty() && workspace.project().frames().empty());
    workspace.document().beginStroke(); workspace.document().paintLine({0,0},{95,95},Ink);
    CHECK(workspace.dirty()); workspace.sync(); CHECK(workspace.project().frames().empty());
    Key second=first; second.pose.clip=Clip::Walk; second.pose.frame=2;
    CHECK(workspace.select(second)); CHECK(workspace.dirty());
    CHECK(workspace.project().frames().size()==1); CHECK(workspace.document().undoDepth()==0);
    CHECK(workspace.select(first)); CHECK(workspace.document().bitmap().at({0,0})==Ink);
    workspace.markSaved(); CHECK(!workspace.dirty());
    paint(workspace,{95,0}); CHECK(workspace.dirty());
    CHECK(workspace.document().undo()); workspace.sync(); CHECK(!workspace.dirty());
    CHECK(workspace.document().redo()); workspace.sync(); CHECK(workspace.dirty());
    CHECK(workspace.select(second)); workspace.resetFrame(); CHECK(workspace.dirty()); // Other frame still dirty.
    paint(workspace); CHECK(workspace.project().frames().size()==2);
    workspace.document().markSaved(); CHECK(workspace.dirty()); // A frame PNG is NOT a project save.
    workspace.markSaved(); CHECK(!workspace.dirty());
    workspace.resetFrame(); CHECK(workspace.dirty()); CHECK(workspace.project().frames().size()==1);
    CHECK(workspace.document().undo()); workspace.sync(); CHECK(!workspace.dirty());
    CHECK(workspace.rename("new-project")); CHECK(workspace.dirty());
    CHECK(!workspace.rename("../bad")); CHECK(workspace.rename("equipment")); CHECK(!workspace.dirty());
    // No-op edits and canceled strokes do not invalidate the project save point.
    CHECK(!workspace.document().paste(workspace.document().bitmap())); workspace.sync(); CHECK(!workspace.dirty());
    workspace.document().paintLine({1,0},{1,0},Ink); workspace.document().cancelStroke(); workspace.sync(); CHECK(!workspace.dirty());
    auto project=workspace.project(); workspace.open(project); CHECK(!workspace.dirty());
    CHECK(workspace.document().undoDepth()==0 && workspace.project()==project);
    Key invalid=first; invalid.pose.frame=60; CHECK(!workspace.select(invalid)); CHECK(!workspace.dirty());
}
void frameReplacement() {
    sprite::Document doc{sprite::Bitmap{96,96}};
    sprite::Bitmap wrong(32,32); CHECK(!doc.paste(wrong) && !doc.dirty());
    sprite::Bitmap frame(96,96); frame.pixels.front()=Ink; frame.pixels.back()=Ink;
    CHECK(doc.paste(frame) && doc.dirty() && doc.undoDepth()==1);
    CHECK(doc.bitmap()==frame); CHECK(doc.undo() && !doc.dirty()); CHECK(doc.redo() && doc.bitmap()==frame);
}
void fixChecksum(std::vector<unsigned char>& bytes) {
    const std::size_t start=37+bytes[36],length=bytes.size()-start;
    for (unsigned i=0;i<4;++i) bytes[28+i]=static_cast<unsigned char>((length>>(8*i))&255);
    std::uint32_t hash=2166136261u;
    for (std::size_t i=start;i<bytes.size();++i) { hash^=bytes[i]; hash*=16777619u; }
    for (unsigned i=0;i<4;++i) bytes[32+i]=static_cast<unsigned char>((hash>>(8*i))&255);
}
void hostileFiles() {
    Project project; Frame flat{}; Key key{};
    CHECK(project.set(key,flat)); // Explicitly empty gear frame must survive a save.
    const auto original=encode(project); const std::size_t begin=37+original[36];
    CHECK(original[begin+5]==1); // RLE for a blank frame.
    std::string error;
    auto blank=decode(original.data(),original.size(),error); CHECK(blank && blank->frame(key)==flat);
    CHECK(!decode(nullptr,0,error) && !error.empty());
    CHECK(!decode(original.data(),MaxFileBytes+1,error));
    for (std::size_t n=0;n<original.size();++n) CHECK(!decode(original.data(),n,error));
    auto bytes=original;
    for (std::size_t i=0;i<37;++i) {
        bytes=original; bytes[i]^=128; CHECK(!decode(bytes.data(),bytes.size(),error));
    }
    const auto reject=[&](std::vector<unsigned char> data) { fixChecksum(data); CHECK(!decode(data.data(),data.size(),error)); };
    bytes=original; bytes[37]='/'; reject(bytes);
    for (std::size_t offset=0;offset<6;++offset) { bytes=original; bytes[begin+offset]=255; reject(bytes); }
    bytes=original; bytes[begin+10]=0; bytes[begin+11]=0; reject(bytes); // Empty run.
    bytes=original; bytes[begin+10]=255; bytes[begin+11]=255; reject(bytes); // Overflow run.
    bytes=original; bytes[begin+10]=1; bytes[begin+11]=0; reject(bytes); // Incomplete frame.
    bytes=original; bytes[begin+6]=0; reject(bytes); // Zero payload length.
    bytes=original; bytes[begin+6]=5; reject(bytes); // Fractional run.
    bytes=original; bytes[begin+5]=0; reject(bytes); // Truncated raw frame.
    bytes=original; bytes.push_back(0); reject(bytes); // Trailing bytes.
    bytes=original; bytes[24]=2; bytes.insert(bytes.end(),original.begin()+static_cast<std::ptrdiff_t>(begin),original.end()); reject(bytes); // Duplicate identity.
    // Raw path: checksum-valid lengths and identities are still independently validated.
    Frame noise{}; for (std::size_t i=0;i<noise.size();++i) noise[i]={static_cast<std::uint8_t>(i%256),255,0,255};
    project.set(key,noise); const auto raw=encode(project); CHECK(raw[begin+5]==0);
    bytes=raw; bytes[begin+6]=1; reject(bytes);
    // Deterministic mutation corpus, many of them with valid checksums.
    std::uint32_t seed=45719;
    for (int trial=0;trial<1200;++trial) {
        bytes=original; seed=seed*1664525u+1013904223u;
        const std::size_t where=seed%bytes.size(); bytes[where]^=static_cast<unsigned char>(1u<<((seed>>16)%8));
        if (trial%2 && where>=begin) fixChecksum(bytes);
        const auto result=decode(bytes.data(),bytes.size(),error);
        if (result) {
            CHECK(validName(result->name));
            for (const auto& entry : result->frames()) CHECK(entry.first.valid() && entry.second);
        } else CHECK(!error.empty());
    }
}
}
int main() {
    int failed=0;
    const std::pair<const char*,std::function<void()>> tests[]={{"catalog identities and registration",identities},
        {"portable project round trip",projectRoundTrip},{"workspace transactions/save points",workspaceTransactions},
        {"undoable full-frame replacement",frameReplacement},{"bounded untrusted file validation",hostileFiles}};
    for (const auto& test : tests) {
        try { test.second(); std::cout<<"[PASS] "<<test.first<<'\n'; }
        catch (const std::exception& error) { ++failed; std::cerr<<"[FAIL] "<<test.first<<": "<<error.what()<<'\n'; }
    }
    std::cout<<assertions<<" assertions, "<<failed<<" failures\n"; return failed?1:0;
}

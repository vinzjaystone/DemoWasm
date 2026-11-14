#pragma once

#include "character_sprites.h"
#include "sprite_document.h"
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

namespace ashfall::art {

// File identities refer to the versioned built-in catalog, never to gameplay
// stats or filenames. A project contains only full, untrimmed edited frames.
constexpr std::size_t MaxFrames=3*3*character::DirectionCount*character::FrameColumns;
constexpr std::size_t MaxFileBytes=50u*1024u*1024u;
struct Key {
    EquipmentSlot slot=EquipmentSlot::Armor;
    int variant=1;
    character::Pose pose{character::Clip::Idle,character::Direction::SouthEast,0};
    auto tuple() const { return std::make_tuple(slot,variant,pose.clip,pose.direction,pose.frame); }
    bool operator<(const Key& other) const { return tuple()<other.tuple(); }
    bool operator==(const Key& other) const { return tuple()==other.tuple(); }
    bool operator!=(const Key& other) const { return !(*this==other); }
    bool valid() const;
};
character::Layer layer(EquipmentSlot slot);
character::Frame starter(Key key);
sprite::Bitmap bitmap(const character::Frame& pixels);
character::Frame pixels(const sprite::Bitmap& bitmap);
bool validName(const std::string& name);

// Immutable frames make project/save-point copies cheap and prevent accidental
// aliasing while editing. No GPU, file, browser, or simulation dependency.
class Project {
public:
    using Frames=std::map<Key,std::shared_ptr<const character::Frame>>;
    std::string name="equipment";
    const Frames& frames() const { return frames_; }
    const character::Frame* find(Key key) const;
    character::Frame frame(Key key) const;
    bool set(Key key,character::Frame pixels); // Unchanged starter frames need no override.
    bool operator==(const Project& other) const;
    bool operator!=(const Project& other) const { return !(*this==other); }
private:
    Frames frames_;
};
bool hasSignature(const unsigned char* bytes,std::size_t size);
std::vector<unsigned char> encode(const Project& project);
std::optional<Project> decode(const unsigned char* bytes,std::size_t size,std::string& error);

// One bounded 64-step undo history for the active frame, not 64 histories for
// every cell. Switching frames commits edits; it never discards the project.
class Workspace {
public:
    Workspace();
    sprite::Document& document() { return document_; }
    const sprite::Document& document() const { return document_; }
    const Project& project() const { return project_; }
    Key key() const { return key_; }
    bool dirty() const { return recovered_ || dirty_ || document_.dirty(); }
    void sync();
    bool select(Key key);
    void open(Project project);
    bool recover(Project project,Key key); // No exported save point until the whole project is saved.
    void markSaved();
    bool rename(const std::string& name);
    void resetFrame();
private:
    Project project_,saved_;
    Key key_;
    sprite::Document document_;
    bool dirty_=false,recovered_=false;
    std::uint64_t syncedStamp_=0;
};

} // namespace ashfall::art

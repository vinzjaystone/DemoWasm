#pragma once

#include "equipment.h"
#include <array>
#include <optional>

namespace ashfall {

// Value-owned, fixed capacity. A full bag can still swap an item with a worn
// one: the displaced item goes in the SAME cell, without a temporary free slot.
class Backpack {
public:
    static constexpr std::size_t Capacity=24;
    using Cells=std::array<std::optional<GearItem>,Capacity>;
    static Backpack starter();
    const Cells& cells() const { return cells_; }
    std::size_t size() const;
    const GearItem* find(std::uint32_t id) const;
    bool insert(GearItem item);
    bool place(std::size_t index,GearItem item); // Checked exact-cell restoration.
    std::optional<GearItem> remove(std::uint32_t id);
    bool equip(std::uint32_t id,Equipment& equipment);
    bool unequip(EquipmentSlot slot,Equipment& equipment);
    bool cycleOwned(EquipmentSlot slot,Equipment& equipment);
    void sort();
private:
    Cells cells_{};
};

enum class LootKind : std::uint8_t { Gear, Embers, Flask };
struct GroundLoot {
    std::uint32_t id=0;
    LootKind kind=LootKind::Gear;
    Vec2 position{};
    int amount=1;
    GearItem gear{}; // Only meaningful for Gear, where gear.id == id.
    std::string name() const;
    Rgb color() const;
};
constexpr std::size_t MaxGroundLoot=320; // Enough for 64 four-drop Wardens + all starter items.
constexpr int MaxEmbers=10000000;
constexpr int MaxFlasks=3;
constexpr int FlaskCraftCost=30;
constexpr float PickupRadius=1.1f;
constexpr std::uint32_t InitialLootSeed=0x0a51fa11u;

// A dedicated, persisted stream. Particles and rendering never
// consume the gear stream; save/load never rerolls rewards; no std::rand or clock dependency.
std::uint32_t lootRandom(std::uint32_t& state);
GearItem rollGear(std::uint32_t id,bool warden,bool guaranteedRelic,std::uint32_t& state);

} // namespace ashfall

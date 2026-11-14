#pragma once

#include "types.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ashfall {

enum class EquipmentSlot : std::size_t { Head = 0, Armor = 1, Boots = 2 };
enum class Rarity : std::uint8_t { Common, Tempered, Relic };
constexpr std::array<EquipmentSlot,3> EquipmentSlots{{EquipmentSlot::Head,EquipmentSlot::Armor,EquipmentSlot::Boots}};
const char* slotName(EquipmentSlot slot);
const char* rarityName(Rarity rarity);
Rgb rarityColor(Rarity rarity);
struct EquipmentPiece {
    std::string_view name;
    std::string_view description;
    int armor = 0;
    float speedBonus = 0;
    Rgb tint;
    int variant = 0;
    float attackSpeedBonus = 0; // Additive multiplier bonus: +0.10 means +10% ASPD.
};

// An owned instance, not an artwork/catalog entry. Integer rolls keep saves and
// comparisons exact; speed is hundredths of a tile/sec, haste is percentage points.
struct GearItem {
    std::uint32_t id = 0;
    EquipmentSlot slot = EquipmentSlot::Head;
    int variant = 0;
    Rarity rarity = Rarity::Common;
    int armorRoll = 0, speedRoll = 0, hasteRoll = 0;
    bool valid() const;
    const EquipmentPiece& base() const;
    std::string name() const;
    int armor() const;
    float speedBonus() const;
    float attackSpeedBonus() const;
    int salvageValue() const;
    bool operator==(const GearItem& other) const;
};

class Equipment {
public:
    static constexpr std::size_t VariantsPerSlot = 3;
    static const EquipmentPiece& piece(EquipmentSlot slot,int variant);
    const EquipmentPiece& get(EquipmentSlot slot) const; // Empty slots have zero stats, variant -1.
    const GearItem* item(EquipmentSlot slot) const;
    bool set(EquipmentSlot slot,std::optional<GearItem> item);
    // Catalog preview helper for the artwork editor/CPU fixtures ONLY. The game
    // uses Backpack::cycleOwned: it must never manufacture items via this API.
    void cycle(EquipmentSlot slot);
    int armor() const;
    float speedBonus() const;
    float attackSpeedBonus() const;
    int variant(EquipmentSlot slot) const;
private:
    std::array<std::optional<GearItem>,3> equipped_{{
        GearItem{1,EquipmentSlot::Head,0},GearItem{2,EquipmentSlot::Armor,1},GearItem{3,EquipmentSlot::Boots,0}
    }};
};

} // namespace ashfall

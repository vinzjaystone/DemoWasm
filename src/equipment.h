#pragma once

#include "types.h"
#include <array>
#include <cstddef>
#include <string_view>

namespace ashfall {

enum class EquipmentSlot : std::size_t { Head = 0, Armor = 1, Boots = 2 };
struct EquipmentPiece {
    std::string_view name;
    std::string_view description;
    int armor = 0;
    float speedBonus = 0;
    Rgb tint;
    int variant = 0;
    float attackSpeedBonus = 0; // Additive multiplier bonus: +0.10 means +10% ASPD.
};

class Equipment {
public:
    static constexpr std::size_t VariantsPerSlot = 3;
    static const EquipmentPiece& piece(EquipmentSlot slot,int variant);
    const EquipmentPiece& get(EquipmentSlot slot) const;
    void cycle(EquipmentSlot slot);
    int armor() const;
    float speedBonus() const;
    float attackSpeedBonus() const;
    int variant(EquipmentSlot slot) const;
private:
    std::array<std::size_t, 3> equipped_{{0, 1, 0}};
};

} // namespace ashfall

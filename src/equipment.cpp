#include "equipment.h"

namespace ashfall {
namespace {
constexpr std::array<std::array<EquipmentPiece, 3>, 3> Catalog = {{
    {{{"Wayfarer's hood", "A little shelter from the ash.", 1, 0.0f, {123, 139, 117}, 0},
      {"Iron sallet", "Forged for an oath long forgotten.", 4, -0.12f, {154, 172, 177}, 1, -0.05f},
      {"Cinder crown", "The old flame remembers its king.", 2, 0.12f, {221, 167, 79}, 2, 0.05f}}},
    {{{"Ranger's leathers", "Light armor. A quiet step.", 2, 0.15f, {139, 99, 70}, 0, 0.05f},
      {"Ashguard plate", "Stand where the others fell.", 6, -0.15f, {99, 135, 135}, 1},
      {"Crimson mantle", "Woven with the last light of dusk.", 3, 0.25f, {167, 73, 70}, 2, 0.10f}}},
    {{{"Pilgrim wraps", "Every journey begins in dust.", 1, 0.0f, {149, 123, 90}, 0},
      {"Iron greaves", "Heavy footsteps. Steady resolve.", 3, -0.20f, {139, 160, 166}, 1, -0.05f},
      {"Fleetstriders", "Leave only embers in your wake.", 1, 0.65f, {115, 156, 128}, 2, 0.10f}}}
}};
}
const EquipmentPiece& Equipment::get(EquipmentSlot slot) const {
    const std::size_t index = static_cast<std::size_t>(slot);
    return Catalog.at(index).at(equipped_.at(index));
}
void Equipment::cycle(EquipmentSlot slot) {
    auto& index = equipped_.at(static_cast<std::size_t>(slot));
    index = (index + 1) % VariantsPerSlot;
}
int Equipment::armor() const {
    int total = 0;
    for (std::size_t i = 0; i < equipped_.size(); ++i)
        total += get(static_cast<EquipmentSlot>(i)).armor;
    return total;
}
float Equipment::speedBonus() const {
    float total = 0;
    for (std::size_t i = 0; i < equipped_.size(); ++i)
        total += get(static_cast<EquipmentSlot>(i)).speedBonus;
    return total;
}
float Equipment::attackSpeedBonus() const {
    float total = 0;
    for (std::size_t i = 0; i < equipped_.size(); ++i)
        total += get(static_cast<EquipmentSlot>(i)).attackSpeedBonus;
    return total;
}
int Equipment::variant(EquipmentSlot slot) const { return get(slot).variant; }

} // namespace ashfall

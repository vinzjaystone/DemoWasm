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
const EquipmentPiece& Equipment::piece(EquipmentSlot slot,int variant) {
    return Catalog.at(static_cast<std::size_t>(slot)).at(static_cast<std::size_t>(variant));
}
const char* slotName(EquipmentSlot slot) {
    constexpr std::array<const char*,3> Names{{"HEAD","ARMOR","BOOTS"}};
    const auto index=static_cast<std::size_t>(slot);
    return index<Names.size()?Names[index]:"INVALID";
}
const char* rarityName(Rarity rarity) {
    switch (rarity) { case Rarity::Common: return "COMMON"; case Rarity::Tempered: return "TEMPERED"; case Rarity::Relic: return "RELIC"; }
    return "INVALID";
}
Rgb rarityColor(Rarity rarity) {
    switch (rarity) { case Rarity::Common: return {171,181,164}; case Rarity::Tempered: return {96,190,183}; case Rarity::Relic: return {224,171,88}; }
    return {171,181,164};
}
bool GearItem::valid() const {
    if (id==0 || static_cast<std::size_t>(slot)>=EquipmentSlots.size() || variant<0 || variant>=3) return false;
    switch (rarity) {
        case Rarity::Common: return armorRoll==0 && speedRoll==0 && hasteRoll==0;
        case Rarity::Tempered: return armorRoll>=1 && armorRoll<=2 && speedRoll>=5 && speedRoll<=15 && hasteRoll>=1 && hasteRoll<=3;
        case Rarity::Relic: return armorRoll>=3 && armorRoll<=5 && speedRoll>=15 && speedRoll<=30 && hasteRoll>=3 && hasteRoll<=6;
    }
    return false;
}
const EquipmentPiece& GearItem::base() const { return Equipment::piece(slot,variant); }
std::string GearItem::name() const {
    const char* prefix=rarity==Rarity::Relic?"Relic ":(rarity==Rarity::Tempered?"Tempered ":"");
    return prefix+std::string(base().name);
}
int GearItem::armor() const { return base().armor+armorRoll; }
float GearItem::speedBonus() const { return base().speedBonus+static_cast<float>(speedRoll)*0.01f; }
float GearItem::attackSpeedBonus() const { return base().attackSpeedBonus+static_cast<float>(hasteRoll)*0.01f; }
int GearItem::salvageValue() const { return 5+static_cast<int>(rarity)*10+base().armor; }
bool GearItem::operator==(const GearItem& other) const {
    return id==other.id && slot==other.slot && variant==other.variant && rarity==other.rarity &&
        armorRoll==other.armorRoll && speedRoll==other.speedRoll && hasteRoll==other.hasteRoll;
}
const GearItem* Equipment::item(EquipmentSlot slot) const {
    const auto& equipped=equipped_.at(static_cast<std::size_t>(slot));
    return equipped?&*equipped:nullptr;
}
const EquipmentPiece& Equipment::get(EquipmentSlot slot) const {
    static constexpr EquipmentPiece Empty{"Empty slot","Find and equip an item from your backpack.",0,0,{133,140,130},-1};
    const auto* equipped=item(slot);
    return equipped?equipped->base():Empty;
}
bool Equipment::set(EquipmentSlot slot,std::optional<GearItem> value) {
    const auto index=static_cast<std::size_t>(slot);
    if (index>=equipped_.size() || (value && (!value->valid() || value->slot!=slot))) return false;
    if (value) for (std::size_t i=0;i<equipped_.size();++i)
        if (i!=index && equipped_[i] && equipped_[i]->id==value->id) return false;
    equipped_[index]=value;
    return true;
}
void Equipment::cycle(EquipmentSlot slot) {
    const auto* previous=item(slot);
    const int variant=previous?(previous->variant+1)%static_cast<int>(VariantsPerSlot):0;
    const std::uint32_t id=previous?previous->id:static_cast<std::uint32_t>(slot)+1u;
    set(slot,GearItem{id,slot,variant});
}
int Equipment::armor() const {
    int total=0; for (auto slot:EquipmentSlots) if (const auto* equipped=item(slot)) total+=equipped->armor(); return total;
}
float Equipment::speedBonus() const {
    float total=0; for (auto slot:EquipmentSlots) if (const auto* equipped=item(slot)) total+=equipped->speedBonus(); return total;
}
float Equipment::attackSpeedBonus() const {
    float total=0; for (auto slot:EquipmentSlots) if (const auto* equipped=item(slot)) total+=equipped->attackSpeedBonus(); return total;
}
int Equipment::variant(EquipmentSlot slot) const { const auto* equipped=item(slot); return equipped?equipped->variant:-1; }

} // namespace ashfall

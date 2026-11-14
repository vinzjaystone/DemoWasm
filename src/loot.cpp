#include "loot.h"
#include <algorithm>
#include <tuple>

namespace ashfall {
Backpack Backpack::starter() {
    Backpack bag;
    bag.insert({4,EquipmentSlot::Head,1});
    bag.insert({5,EquipmentSlot::Armor,2});
    bag.insert({6,EquipmentSlot::Boots,1});
    return bag;
}
std::size_t Backpack::size() const {
    return static_cast<std::size_t>(std::count_if(cells_.begin(),cells_.end(),[](const auto& cell) { return cell.has_value(); }));
}
const GearItem* Backpack::find(std::uint32_t id) const {
    for (const auto& cell:cells_) if (cell && cell->id==id) return &*cell;
    return nullptr;
}
bool Backpack::insert(GearItem item) {
    const auto free=std::find(cells_.begin(),cells_.end(),std::nullopt);
    return free!=cells_.end() && place(static_cast<std::size_t>(free-cells_.begin()),item);
}
bool Backpack::place(std::size_t index,GearItem item) {
    if (index>=cells_.size() || cells_[index] || !item.valid() || find(item.id)) return false;
    cells_[index]=item;
    return true;
}
std::optional<GearItem> Backpack::remove(std::uint32_t id) {
    for (auto& cell:cells_) if (cell && cell->id==id) { const auto item=cell; cell.reset(); return item; }
    return std::nullopt;
}
bool Backpack::equip(std::uint32_t id,Equipment& equipment) {
    for (auto& cell:cells_) if (cell && cell->id==id) {
        // Reject an inconsistent ownership graph instead of duplicating a worn instance.
        for (auto slot:EquipmentSlots) if (const auto* worn=equipment.item(slot); worn && worn->id==id) return false;
        const GearItem candidate=*cell;
        const auto* worn=equipment.item(candidate.slot);
        const std::optional<GearItem> previous=worn?std::optional<GearItem>{*worn}:std::nullopt;
        if (previous && find(previous->id)) return false;
        if (!equipment.set(candidate.slot,candidate)) return false;
        cell=previous;
        return true;
    }
    return false;
}
bool Backpack::unequip(EquipmentSlot slot,Equipment& equipment) {
    if (static_cast<std::size_t>(slot)>=EquipmentSlots.size()) return false;
    const auto* worn=equipment.item(slot);
    if (!worn || !insert(*worn)) return false;
    equipment.set(slot,std::nullopt);
    return true;
}
bool Backpack::cycleOwned(EquipmentSlot slot,Equipment& equipment) {
    if (static_cast<std::size_t>(slot)>=EquipmentSlots.size()) return false;
    const auto* worn=equipment.item(slot);
    const auto key=[](const GearItem& item) { return std::make_tuple(item.variant,item.rarity,item.id); };
    const GearItem* first=nullptr;
    const GearItem* next=nullptr;
    for (const auto& cell:cells_) if (cell && cell->slot==slot) {
        if (!first || key(*cell)<key(*first)) first=&*cell;
        if ((!worn || key(*cell)>key(*worn)) && (!next || key(*cell)<key(*next))) next=&*cell;
    }
    const auto* chosen=next?next:first;
    return chosen && equip(chosen->id,equipment);
}
void Backpack::sort() {
    std::stable_sort(cells_.begin(),cells_.end(),[](const auto& a,const auto& b) {
        if (!a || !b) return a.has_value() && !b;
        return std::make_tuple(a->slot,-static_cast<int>(a->rarity),a->variant,a->id)<
               std::make_tuple(b->slot,-static_cast<int>(b->rarity),b->variant,b->id);
    });
}
std::string GroundLoot::name() const {
    switch (kind) {
        case LootKind::Gear: return gear.name();
        case LootKind::Embers: return std::to_string(amount)+" embers";
        case LootKind::Flask: return "Healing flask";
    }
    return "Unknown loot";
}
Rgb GroundLoot::color() const {
    if (kind==LootKind::Gear) return rarityColor(gear.rarity);
    return kind==LootKind::Flask?Rgb{104,185,151}:Rgb{224,171,88};
}
std::uint32_t lootRandom(std::uint32_t& state) {
    state^=state<<13; state^=state>>17; state^=state<<5;
    return state;
}
GearItem rollGear(std::uint32_t id,bool warden,bool guaranteedRelic,std::uint32_t& state) {
    GearItem item; item.id=id;
    item.slot=static_cast<EquipmentSlot>(lootRandom(state)%3u);
    item.variant=static_cast<int>(lootRandom(state)%3u);
    const auto roll=lootRandom(state)%100u;
    item.rarity=guaranteedRelic?Rarity::Relic:(roll<3u?Rarity::Relic:((warden || roll<28u)?Rarity::Tempered:Rarity::Common));
    if (item.rarity==Rarity::Tempered) {
        item.armorRoll=1+static_cast<int>(lootRandom(state)%2u);
        item.speedRoll=5+static_cast<int>(lootRandom(state)%11u);
        item.hasteRoll=1+static_cast<int>(lootRandom(state)%3u);
    } else if (item.rarity==Rarity::Relic) {
        item.armorRoll=3+static_cast<int>(lootRandom(state)%3u);
        item.speedRoll=15+static_cast<int>(lootRandom(state)%16u);
        item.hasteRoll=3+static_cast<int>(lootRandom(state)%4u);
    }
    return item;
}
} // namespace ashfall

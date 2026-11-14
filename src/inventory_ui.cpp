#include "renderer.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace ashfall {
namespace {
constexpr Color Back{12,16,18,255},Panel{18,23,25,255},Raised{25,31,32,255},Border{45,53,53,255};
constexpr Color Ink{222,224,208,255},Muted{126,140,135,255},Gold{211,171,104,255},Teal{104,177,157,255},Red{200,78,65,255};
Color color(Rgb rgb) { return {rgb.r,rgb.g,rgb.b,255}; }
float width(const std::string& value,float size=10) { return MeasureTextEx(GetFontDefault(),value.c_str(),size,0.5f).x; }
void label(const std::string& value,float x,float y,float size=10,Color ink=Ink) {
    DrawTextEx(GetFontDefault(),value.c_str(),{x,y},size,0.5f,ink);
}
void fit(std::string value,Rectangle box,float size,Color ink) {
    if (width(value,size)>box.width) {
        while (!value.empty() && width(value+"...",size)>box.width) value.pop_back();
        value+="...";
    }
    label(value,box.x,box.y,size,ink);
}
void centered(const std::string& value,Rectangle box,float size=10,Color ink=Ink) {
    label(value,box.x+(box.width-width(value,size))*0.5f,box.y+(box.height-size)*0.5f,size,ink);
}
void button(Rectangle box,const std::string& value,bool enabled=true,bool primary=false) {
    const bool hover=enabled && CheckCollisionPointRec(GetMousePosition(),box);
    DrawRectangleRec(box,enabled?(primary?Gold:(hover?Raised:Back)):Panel);
    DrawRectangleLinesEx(box,1,enabled?(primary?Gold:Border):Border);
    centered(value,box,10,enabled?(primary?Back:Ink):Muted);
}
void glyph(EquipmentSlot slot,Vector2 center,float size,Color tint) {
    const float x=center.x,y=center.y,s=size*0.5f;
    if (slot==EquipmentSlot::Head) {
        DrawRectangleRec({x-s*0.7f,y-s*0.5f,s*1.4f,s*1.2f},tint);
        DrawTriangle({x-s*0.7f,y-s*0.5f},{x+s*0.7f,y-s*0.5f},{x,y-s},tint);
        DrawRectangleRec({x-s*0.5f,y,s,s*0.22f},Back);
    } else if (slot==EquipmentSlot::Armor) {
        DrawRectangleRec({x-s*0.6f,y-s*0.7f,s*1.2f,s*1.5f},tint);
        DrawRectangleRec({x-s,y-s*0.6f,s*0.55f,s*0.6f},tint);
        DrawRectangleRec({x+s*0.45f,y-s*0.6f,s*0.55f,s*0.6f},tint);
        DrawRectangleRec({x-s*0.25f,y-s*0.8f,s*0.5f,s*0.35f},Back);
    } else {
        for (float dx:{-0.55f,0.55f}) {
            DrawRectangleRec({x+s*dx-s*0.18f,y-s*0.9f,s*0.45f,s*1.4f},tint);
            DrawRectangleRec({x+s*dx-s*0.18f,y+s*0.25f,s*0.7f,s*0.45f},tint);
        }
    }
}
void cell(Rectangle box,const GearItem* item,bool selected,bool worn) {
    const bool hover=CheckCollisionPointRec(GetMousePosition(),box);
    DrawRectangleRec(box,selected?Raised:Back);
    const Color edge=item?color(rarityColor(item->rarity)):Border;
    DrawRectangleLinesEx(box,selected?2.0f:1.0f,hover?Gold:edge);
    if (!item) { centered(worn?"EMPTY":".",box,10,Muted); return; }
    const bool wide=box.width>90;
    glyph(item->slot,{box.x+(wide?24:box.width*0.5f),box.y+box.height*0.49f},std::min(30.0f,box.height*0.55f),color(item->base().tint));
    if (wide) {
        fit(slotName(item->slot),{box.x+44,box.y+8,box.width-48,12},9,Muted);
        fit(std::string(item->base().name),{box.x+44,box.y+24,box.width-48,16},10,Ink);
    } else {
        const char* rarity=item->rarity==Rarity::Relic?"R":(item->rarity==Rarity::Tempered?"T":"C");
        label(rarity,box.x+5,box.y+4,9,edge);
        label(std::to_string(item->variant+1),box.x+box.width-11,box.y+box.height-13,9,Muted);
    }
    if (worn && !wide) label(slotName(item->slot),box.x+4,box.y+box.height-11,8,Muted);
}
}

void Renderer::drawBackpack(const World& world,const ViewState& view) const {
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{5,9,11,220});
    const auto& l=layout_;
    const Rectangle panel=l.bagPanel,detail=l.bagDetails;
    const bool compact=panel.height<540;
    DrawRectangleRec(panel,Panel); DrawRectangleLinesEx(panel,1,Border);
    DrawRectangleRec({panel.x,panel.y,panel.width,2},Gold);
    label("BACKPACK",panel.x+20,panel.y+19,22,Ink);
    label("COMBAT & MOVEMENT PAUSED",panel.x+20,panel.y+47,9,Teal);
    button(l.bagClose,"CLOSE [B/ESC]");
    for (auto slot:EquipmentSlots) {
        cell(l.bagWorn[static_cast<std::size_t>(slot)],world.player.equipment.item(slot),
            world.player.equipment.item(slot) && world.player.equipment.item(slot)->id==view.selectedItem,true);
    }
    const float gridY=l.bagCells.front().y;
    const auto used=world.player.backpack.size();
    label("OWNED GEAR  "+std::to_string(used)+" / 24",l.bagCells.front().x,gridY-22,10,Gold);
    for (std::size_t i=0;i<Backpack::Capacity;++i) {
        const auto& item=world.player.backpack.cells()[i];
        cell(l.bagCells[i],item?&*item:nullptr,item && item->id==view.selectedItem,false);
    }
    const auto last=l.bagCells.back();
    label(std::to_string(world.player.embers)+" EMBERS    "+std::to_string(world.player.potions)+" / 3 FLASKS",
        panel.x+20,last.y+last.height+10,10,Gold);
    if (!compact) {
        label("Select a cell to compare with the worn item.",panel.x+20,last.y+last.height+38,10,Muted);
        label("Swaps work even when the backpack is full.",panel.x+20,last.y+last.height+57,10,Muted);
        label("Drop is reversible. Salvage asks for confirmation.",panel.x+20,last.y+last.height+76,10,Muted);
        label("1 / 2 / 3 rotate only spares you actually own.",panel.x+20,last.y+last.height+95,10,Muted);
    }
    const auto* selected=world.ownedItem(view.selectedItem);
    const bool worn=world.wornItem(view.selectedItem);
    DrawLineEx({detail.x-11,detail.y},{detail.x-11,panel.y+panel.height-44},1,Border);
    if (selected) {
        const Color rarity=color(rarityColor(selected->rarity));
        label(std::string(rarityName(selected->rarity))+" / "+slotName(selected->slot),detail.x,detail.y,10,rarity);
        fit(std::string(selected->base().name),{detail.x,detail.y+23,detail.width,30},compact?17.0f:21.0f,Ink);
        fit(std::string(selected->base().description),{detail.x,detail.y+52,detail.width,16},10,Muted);
        const auto* equipped=world.player.equipment.item(selected->slot);
        fit(worn?"CURRENTLY EQUIPPED":("COMPARE TO "+(equipped?std::string(equipped->base().name):"EMPTY SLOT")),
            {detail.x,detail.y+80,detail.width,16},9,worn?Gold:Teal);
        const int deltaArmor=selected->armor()-(equipped?equipped->armor():0);
        const float deltaSpeed=selected->speedBonus()-(equipped?equipped->speedBonus():0);
        const float deltaHaste=selected->attackSpeedBonus()-(equipped?equipped->attackSpeedBonus():0);
        const auto stat=[&](const char* title,const char* value,const char* delta,float change,int row) {
            const float y=detail.y+108+static_cast<float>(row)*25;
            label(title,detail.x,y,10,Muted);
            label(value,detail.x+detail.width*0.40f,y,12,Ink);
            if (!worn) {
                const std::string difference=delta;
                label(difference,detail.x+detail.width-width(difference),y,10,change>0.001f?Teal:(change<-0.001f?Red:Muted));
            }
        };
        char value[32],delta[32];
        std::snprintf(value,sizeof(value),"%d",selected->armor()); std::snprintf(delta,sizeof(delta),"%+d",deltaArmor);
        stat("ARMOR",value,delta,static_cast<float>(deltaArmor),0);
        std::snprintf(value,sizeof(value),"%+.2f",static_cast<double>(selected->speedBonus())); std::snprintf(delta,sizeof(delta),"%+.2f",static_cast<double>(deltaSpeed));
        stat("MOVE",value,delta,deltaSpeed,1);
        std::snprintf(value,sizeof(value),"%+d%%",static_cast<int>(std::lround(selected->attackSpeedBonus()*100)));
        std::snprintf(delta,sizeof(delta),"%+d%%",static_cast<int>(std::lround(deltaHaste*100)));
        stat("ASPD",value,delta,deltaHaste,2);
        fit(selected->rarity==Rarity::Common?"Catalog stats / no rolled bonuses":"Stats include this instance's rolled bonuses",
            {detail.x,detail.y+183,detail.width,16},9,rarity);
    } else {
        label("SELECT AN ITEM",detail.x,detail.y+23,18,Muted);
        fit("Inspect owned gear or a worn slot.",{detail.x,detail.y+64,detail.width,20},10,Muted);
    }
    button(l.bagEquip,worn?"UNEQUIP TO BACKPACK":"EQUIP / SWAP",selected!=nullptr,true);
    button(l.bagDrop,"DROP",selected && !worn);
    std::string salvage="SALVAGE";
    if (selected && !worn) salvage=(view.confirmSalvage==selected->id?"CONFIRM +":"SALVAGE +")+std::to_string(selected->salvageValue());
    button(l.bagSalvage,salvage,selected && !worn);
    button(l.bagCraft,"CRAFT FLASK / 30 EMBERS",world.player.potions<MaxFlasks && world.player.embers>=FlaskCraftCost);
    button(l.bagSort,"SORT: SLOT / RARITY / BASE ITEM");
    const std::string message=world.noticeRemaining>0?world.notice:"Embers craft healing flasks. Uncollected drops never expire.";
    fit(message,{panel.x+20,panel.y+panel.height-21,panel.width-40,14},10,Gold);
}
} // namespace ashfall

#pragma once

#include "character_animation.h"
#include <array>
#include <vector>

namespace ashfall::character {

// Original, procedural starter art. Each logical frame is a transparent 96px
// square with a common foot pivot. Cropping only removes shared empty margins
// when packing GPU sheets; it never changes the pivot or frame registration.
constexpr int CellSize = 96;
constexpr int FootX = 48, FootY = 76;
constexpr int FrameColumns = 18; // 4 idle + 8 walk + 6 attack
struct Pixel { std::uint8_t r=0,g=0,b=0,a=0; };
static_assert(sizeof(Pixel)==4,"The atlas is tightly packed RGBA8");
using Frame = std::array<Pixel,CellSize*CellSize>;
enum class Layer : std::uint8_t { Base, Boots, Armor, Head, Weapon, Count };
constexpr std::size_t LayerCount = static_cast<std::size_t>(Layer::Count);
struct SheetInfo {
    const char* name;
    int x, y, cellWidth, cellHeight, variants;
    int width() const { return FrameColumns*cellWidth; }
    int height() const { return DirectionCount*variants*cellHeight; }
};
struct CellRect { int x,y,width,height; };
const SheetInfo& sheetInfo(Layer layer);
CellRect sourceCell(Layer layer, Pose pose, int variant=0);
Frame makeStarterFrame(Layer layer, Pose pose, int variant=0);
// One temporary allocation per sheet at startup, released after GPU upload.
std::vector<Pixel> makeStarterSheet(Layer layer);

} // namespace ashfall::character

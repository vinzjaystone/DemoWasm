#pragma once

#include "snapshot_codec.h"
#include "world.h"
#include <optional>

namespace ashfall::save {
constexpr snapshot::Magic RunMagic{{'A','S','H','R','U','N',0,0}};
constexpr std::size_t MaxRunBytes=128u*1024u;
// Rules v2 includes owned gear/backpack/ground loot and migrates original v1
// runs. Both identify the bundled sanctum/catalog, not arbitrary map data.
// Serialize complete gameplay state, including in-flight swings, cooldowns,
// target IDs and navigation. Presentation particles/notice/camera are excluded.
snapshot::Bytes encodeRun(const World& world,std::string& error);
std::optional<World> decodeRun(const unsigned char* bytes,std::size_t size,std::string& error);
} // namespace ashfall::save

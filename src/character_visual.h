#pragma once

#include "character_sprites.h"
#include "raylib.h"
#include <array>

namespace ashfall::character {

// Owns only GPU presentation resources. Initialize after InitWindow and destroy
// before CloseWindow. Failed/disabled loading leaves the procedural hero usable.
class Sprites {
public:
    Sprites() = default;
    ~Sprites();
    Sprites(const Sprites&) = delete;
    Sprites& operator=(const Sprites&) = delete;
    bool load();
    bool ready() const { return ready_; }
    int uploads() const { return uploads_; }
    bool draw(Vector2 feet,Pose pose,const Equipment& equipment,float scale,
              bool flash=false,Color wash=WHITE) const;
private:
    std::array<Texture2D,LayerCount> textures_{};
    Shader flashShader_{};
    int flashLocation_=-1,uploads_=0;
    bool attempted_=false,ready_=false;
    void release();
    void drawLayer(Layer layer,Vector2 feet,Pose pose,int variant,Color tint,float scale) const;
};

} // namespace ashfall::character

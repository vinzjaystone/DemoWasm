#pragma once

#include "equipment_art.h"
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
              bool flash=false,Color wash=WHITE,std::uint32_t visible=31) const;
    bool apply(const art::Project& project); // Atomic replacement; failure preserves current art.
    bool setFrame(art::Key key,const Frame* pixels); // Editor-only incremental GPU update.
    void clearCustom();
    std::size_t customFrames() const { return overrides_.size(); }
private:
    std::array<Texture2D,LayerCount> textures_{};
    std::map<art::Key,Texture2D> overrides_;
    Shader flashShader_{};
    int flashLocation_=-1,uploads_=0;
    bool attempted_=false,ready_=false;
    void release();
    void drawLayer(Layer layer,Vector2 feet,Pose pose,int variant,Color tint,float scale) const;
};

} // namespace ashfall::character

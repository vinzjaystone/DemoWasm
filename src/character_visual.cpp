#include "character_visual.h"
#include "rlgl.h"

namespace ashfall::character {
namespace {
// Silhouette-preserving hit-flash shader; ordinary sprite drawing uses Raylib's
// default shader. Both desktop GL 3.3 and WebGL 1 / GLES2 work.
#ifdef __EMSCRIPTEN__
constexpr const char* FlashShader=R"(#version 100
precision mediump float;
varying vec2 fragTexCoord;
varying vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float hitFlash;
void main() {
    vec4 color=texture2D(texture0,fragTexCoord)*colDiffuse*fragColor;
    gl_FragColor=vec4(mix(color.rgb,vec3(0.93,0.95,0.86),hitFlash),color.a);
})";
#else
constexpr const char* FlashShader=R"(#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform float hitFlash;
out vec4 finalColor;
void main() {
    vec4 color=texture(texture0,fragTexCoord)*colDiffuse*fragColor;
    finalColor=vec4(mix(color.rgb,vec3(0.93,0.95,0.86),hitFlash),color.a);
})";
#endif
// A generated texture id alone does not guarantee RGBA storage was allocated.
// Validate once at import, not while drawing; detach before deleting the probe.
bool allocatedFrame(Texture2D texture) {
    if (!texture.id || texture.width!=CellSize || texture.height!=CellSize) return false;
    const unsigned int previous=rlGetActiveFramebuffer(),probe=rlLoadFramebuffer();
    if (!probe) return false;
    rlFramebufferAttach(probe,texture.id,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
    const bool complete=rlFramebufferComplete(probe);
    rlFramebufferAttach(probe,0,RL_ATTACHMENT_COLOR_CHANNEL0,RL_ATTACHMENT_TEXTURE2D,0);
    rlUnloadFramebuffer(probe); rlEnableFramebuffer(previous);
    return complete;
}
Color multiply(Color a,Color b) {
    return {static_cast<unsigned char>(static_cast<int>(a.r)*b.r/255),
        static_cast<unsigned char>(static_cast<int>(a.g)*b.g/255),
        static_cast<unsigned char>(static_cast<int>(a.b)*b.b/255),
        static_cast<unsigned char>(static_cast<int>(a.a)*b.a/255)};
}
}
Sprites::~Sprites() { release(); }
void Sprites::clearCustom() {
    for (const auto& entry : overrides_) if (entry.second.id) UnloadTexture(entry.second);
    overrides_.clear();
}
bool Sprites::setFrame(art::Key key,const Frame* pixels) {
    if (!ready_ || !key.valid()) return false;
    const auto it=overrides_.find(key);
    if (!pixels) {
        if (it!=overrides_.end()) { UnloadTexture(it->second); overrides_.erase(it); }
        return true;
    }
    if (it!=overrides_.end()) { UpdateTexture(it->second,pixels->data()); return true; }
    Image image{const_cast<Pixel*>(pixels->data()),CellSize,CellSize,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D texture=LoadTextureFromImage(image);
    if (!allocatedFrame(texture)) { if (texture.id) UnloadTexture(texture); return false; }
    SetTextureFilter(texture,TEXTURE_FILTER_POINT); SetTextureWrap(texture,TEXTURE_WRAP_CLAMP);
    overrides_.emplace(key,texture); return true;
}
bool Sprites::apply(const art::Project& project) {
    if (!ready_) return false;
    std::map<art::Key,Texture2D> pending;
    for (const auto& entry : project.frames()) {
        Image image{const_cast<Pixel*>(entry.second->data()),CellSize,CellSize,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        Texture2D texture=LoadTextureFromImage(image);
        if (!allocatedFrame(texture)) {
            if (texture.id) UnloadTexture(texture);
            for (const auto& uploaded : pending) UnloadTexture(uploaded.second);
            return false;
        }
        SetTextureFilter(texture,TEXTURE_FILTER_POINT); SetTextureWrap(texture,TEXTURE_WRAP_CLAMP);
        pending.emplace(entry.first,texture);
    }
    clearCustom(); overrides_.swap(pending); return true;
}
void Sprites::release() {
    clearCustom();
    for (Texture2D& texture : textures_) {
        if (texture.id!=0) UnloadTexture(texture);
        texture={};
    }
    if (flashShader_.id!=0) UnloadShader(flashShader_);
    flashShader_={}; flashLocation_=-1; ready_=false;
}
bool Sprites::load() {
    if (attempted_) return ready_;
    attempted_=true;
    for (std::size_t i=0;i<LayerCount;++i) {
        const Layer layer=static_cast<Layer>(i);
        const SheetInfo& info=sheetInfo(layer);
        auto pixels=makeStarterSheet(layer);
        Image image{pixels.data(),info.width(),info.height(),1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        textures_[i]=LoadTextureFromImage(image);
        // pixels owns the image bytes; do not call UnloadImage on borrowed data.
        if (textures_[i].id==0 || textures_[i].width!=info.width() || textures_[i].height!=info.height()) {
            TraceLog(LOG_WARNING,"CHARACTER: Atlas upload failed; using procedural hero");
            release(); return false;
        }
        SetTextureFilter(textures_[i],TEXTURE_FILTER_POINT);
        SetTextureWrap(textures_[i],TEXTURE_WRAP_CLAMP);
        ++uploads_;
    }
    flashShader_=LoadShaderFromMemory(nullptr,FlashShader);
    flashLocation_=GetShaderLocation(flashShader_,"hitFlash");
    if (flashLocation_<0) {
        TraceLog(LOG_WARNING,"CHARACTER: Hit-flash shader unavailable; using procedural hero");
        release(); return false;
    }
    ready_=true;
    return true;
}
void Sprites::drawLayer(Layer layer,Vector2 feet,Pose pose,int variant,Color color,float scale) const {
    const SheetInfo& info=sheetInfo(layer);
    const CellRect cell=sourceCell(layer,pose,variant);
    const bool gear=layer==Layer::Head || layer==Layer::Armor || layer==Layer::Boots;
    const EquipmentSlot slot=layer==Layer::Head?EquipmentSlot::Head:(layer==Layer::Boots?EquipmentSlot::Boots:EquipmentSlot::Armor);
    const auto custom=gear?overrides_.find({slot,variant,pose}):overrides_.end();
    const bool full=custom!=overrides_.end();
    const Rectangle source=full?Rectangle{0,0,CellSize,CellSize}:Rectangle{static_cast<float>(cell.x),static_cast<float>(cell.y),
        static_cast<float>(cell.width),static_cast<float>(cell.height)};
    const Rectangle destination{feet.x+static_cast<float>((full?0:info.x)-FootX)*scale,
        feet.y+static_cast<float>((full?0:info.y)-FootY)*scale,
        static_cast<float>(full?CellSize:cell.width)*scale,static_cast<float>(full?CellSize:cell.height)*scale};
    DrawTexturePro(full?custom->second:textures_[static_cast<std::size_t>(layer)],source,destination,{0,0},0,color);
}
bool Sprites::draw(Vector2 feet,Pose pose,const Equipment& equipment,float scale,bool flash,Color wash,std::uint32_t visible) const {
    if (!ready_) return false;
    const bool flashing=flash && flashLocation_>=0;
    if (flashing) {
        const float amount=1;
        SetShaderValue(flashShader_,flashLocation_,&amount,SHADER_UNIFORM_FLOAT);
        BeginShaderMode(flashShader_);
    }
    const auto draw=[&](Layer layer,int variant,Color tint) {
        if (visible&(1u<<static_cast<unsigned>(layer))) drawLayer(layer,feet,pose,variant,tint,scale);
    };
    if (weaponBehind(pose)) draw(Layer::Weapon,0,wash);
    draw(Layer::Base,0,wash);
    for (EquipmentSlot slot : {EquipmentSlot::Boots,EquipmentSlot::Armor,EquipmentSlot::Head}) {
        const Layer layer=art::layer(slot);
        const auto& item=equipment.get(slot);
        const Color color{item.tint.r,item.tint.g,item.tint.b,255};
        draw(layer,item.variant,multiply(color,wash));
    }
    if (!weaponBehind(pose)) draw(Layer::Weapon,0,wash);
    if (flashing) EndShaderMode();
    return true;
}

} // namespace ashfall::character

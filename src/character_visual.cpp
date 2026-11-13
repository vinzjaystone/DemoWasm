#include "character_visual.h"

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
Color multiply(Color a,Color b) {
    return {static_cast<unsigned char>(static_cast<int>(a.r)*b.r/255),
        static_cast<unsigned char>(static_cast<int>(a.g)*b.g/255),
        static_cast<unsigned char>(static_cast<int>(a.b)*b.b/255),
        static_cast<unsigned char>(static_cast<int>(a.a)*b.a/255)};
}
}
Sprites::~Sprites() { release(); }
void Sprites::release() {
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
    const Rectangle source{static_cast<float>(cell.x),static_cast<float>(cell.y),
        static_cast<float>(cell.width),static_cast<float>(cell.height)};
    const Rectangle destination{feet.x+static_cast<float>(info.x-FootX)*scale,
        feet.y+static_cast<float>(info.y-FootY)*scale,
        static_cast<float>(cell.width)*scale,static_cast<float>(cell.height)*scale};
    DrawTexturePro(textures_[static_cast<std::size_t>(layer)],source,destination,{0,0},0,color);
}
bool Sprites::draw(Vector2 feet,Pose pose,const Equipment& equipment,float scale,bool flash,Color wash) const {
    if (!ready_) return false;
    const bool flashing=flash && flashLocation_>=0;
    if (flashing) {
        const float amount=1;
        SetShaderValue(flashShader_,flashLocation_,&amount,SHADER_UNIFORM_FLOAT);
        BeginShaderMode(flashShader_);
    }
    if (weaponBehind(pose)) drawLayer(Layer::Weapon,feet,pose,0,wash,scale);
    drawLayer(Layer::Base,feet,pose,0,wash,scale);
    for (EquipmentSlot slot : {EquipmentSlot::Boots,EquipmentSlot::Armor,EquipmentSlot::Head}) {
        const Layer layer=slot==EquipmentSlot::Boots ? Layer::Boots : (slot==EquipmentSlot::Armor ? Layer::Armor : Layer::Head);
        const auto& item=equipment.get(slot);
        const Color color{item.tint.r,item.tint.g,item.tint.b,255};
        drawLayer(layer,feet,pose,item.variant,multiply(color,wash),scale);
    }
    if (!weaponBehind(pose)) drawLayer(Layer::Weapon,feet,pose,0,wash,scale);
    if (flashing) EndShaderMode();
    return true;
}

} // namespace ashfall::character

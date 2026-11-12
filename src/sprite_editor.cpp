#include "sprite_document.h"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#ifndef __EMSCRIPTEN__
#include <filesystem>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
EM_JS(void,downloadSpritePng,(const unsigned char* data,int size,const char* name),{
    const blob=new Blob([HEAPU8.slice(data,data+size)],{type:'image/png'});
    const url=URL.createObjectURL(blob); const link=document.createElement('a');
    link.href=url; link.download=UTF8ToString(name); document.body.appendChild(link); link.click(); link.remove();
    setTimeout(function(){URL.revokeObjectURL(url);},30000);
});
#endif

namespace ashfall::sprite {
namespace {
constexpr Color Background{15,20,24,255}, Panel{21,27,31,255}, Raised{30,38,42,255};
constexpr Color Border{49,59,63,255}, Ink{228,232,220,255}, Muted{136,151,151,255};
constexpr Color Gold{218,178,112,255}, Teal{111,187,166,255};
constexpr std::array<Pixel,28> Palette{{
    {0,0,0,0},{18,25,30,255},{41,54,62,255},{76,93,102,255},{130,148,151,255},{190,205,198,255},{240,237,214,255},
    {77,42,42,255},{132,60,53,255},{207,88,65,255},{237,146,83,255},{218,178,112,255},{240,211,152,255},{255,237,186,255},
    {25,59,62,255},{40,94,91,255},{61,137,117,255},{111,187,166,255},{164,215,179,255},{198,226,186,255},{221,240,215,255},
    {43,44,75,255},{72,72,111,255},{102,122,166,255},{143,173,201,255},{117,78,113,255},{171,116,144,255},{225,173,181,255}
}};
constexpr std::array<float,11> Zooms{{2,3,4,6,8,12,16,20,24,32,40}};
Color color(Pixel p) { return {p.r,p.g,p.b,p.a}; }
Color fade(Color c, float opacity) { c.a=static_cast<unsigned char>(static_cast<float>(c.a)*std::clamp(opacity,0.0f,1.0f)); return c; }
bool inside(Vector2 p, Rectangle r) { return r.width>0 && r.height>0 && CheckCollisionPointRec(p,r); }
void text(const char* value,float x,float y,float size,Color tint=Ink) {
    DrawTextEx(GetFontDefault(),value,{x,y},std::max(10.0f,size),0.6f,tint);
}
float measure(const char* value,float size) { return MeasureTextEx(GetFontDefault(),value,std::max(10.0f,size),0.6f).x; }
void centered(const char* value,Rectangle r,float size=11,Color tint=Ink) {
    text(value,r.x+(r.width-measure(value,size))*0.5f,r.y+(r.height-size)*0.5f,size,tint);
}
void divider(float x,float y,float width) { DrawLineEx({x,y},{x+width,y},1,Border); }
void button(Rectangle r,const char* title,bool active=false,bool disabled=false,bool primary=false) {
    const bool hover=inside(GetMousePosition(),r) && !disabled;
    const Color fill=primary ? Gold : ((hover || active) ? Raised : Panel);
    DrawRectangleRec(r,fill);
    DrawRectangleLinesEx(r,1,active ? Gold : (hover ? Muted : Border));
    centered(title,r,11,primary ? Background : (disabled ? fade(Muted,0.4f) : (active?Gold:Ink)));
}
void checker(Rectangle r,float cell=10) {
    DrawRectangleRec(r,{37,45,51,255});
    const int cols=static_cast<int>(std::ceil(r.width/cell)), rows=static_cast<int>(std::ceil(r.height/cell));
    for (int y=0;y<rows;++y) for (int x=0;x<cols;++x) if ((x+y)%2==0)
        DrawRectangleRec({r.x+static_cast<float>(x)*cell,r.y+static_cast<float>(y)*cell,
            std::min(cell,r.width-static_cast<float>(x)*cell),std::min(cell,r.height-static_cast<float>(y)*cell)},Color{45,54,60,255});
}
void toolIcon(int tool,Vector2 c,Color tint) {
    const auto line=[&](float x,float y,float ex,float ey,float width=2.0f) {
        DrawLineEx({c.x+x,c.y+y},{c.x+ex,c.y+ey},width,tint);
    };
    if (tool==0) {
        line(-7,7,7,-7,5); line(-8,8,-11,11,2); line(4,-9,9,-4,2);
    } else if (tool==1) {
        const Vector2 a{c.x-10,c.y+1},b{c.x+1,c.y-10},d{c.x+10,c.y-1},e{c.x-1,c.y+10};
        DrawTriangle(a,e,d,tint); DrawTriangle(a,d,b,tint); line(-5,6,5,-4,2);
    } else if (tool==2) {
        line(-9,-2,0,-11); line(0,-11,10,-1); line(10,-1,0,9); line(0,9,-9,-2);
        line(-7,0,7,0,3); DrawCircleV({c.x+10,c.y+9},3,tint);
    } else if (tool==3) {
        line(-7,7,7,-7,4); line(-10,10,-6,6,2); line(3,-10,10,-3,2);
        DrawCircleLines(static_cast<int>(c.x+9),static_cast<int>(c.y-9),3,tint);
    } else if (tool==4) {
        line(-9,8,9,-8); DrawCircleV({c.x-9,c.y+8},3,tint); DrawCircleV({c.x+9,c.y-8},3,tint);
    } else DrawRectangleLinesEx({c.x-10,c.y-8,20,16},2,tint);
}
struct OwnedImage {
    Image value{};
    explicit OwnedImage(Image image) : value(image) {}
    ~OwnedImage() { if (value.data) UnloadImage(value); }
    OwnedImage(const OwnedImage&)=delete;
    OwnedImage& operator=(const OwnedImage&)=delete;
};
}

enum class Tool { Pencil, Eraser, Fill, Picker, Line, Rectangle };
enum class Modal { None, New, Import, Leave, Help, Overwrite, OpenHint };
struct Layout {
    Rectangle workspace{}, canvas{}, inspector{}, dock{}, back{}, create{}, open{}, save{};
    Rectangle undo{}, redo{}, mirror{}, grid{}, fit{}, minus{}, plus{}, hex{}, name{}, clear{}, example{}, help{};
    Rectangle dialog{}, cancel{}, confirm{};
    std::array<Rectangle,6> tools{};
    std::array<Rectangle,4> sizes{};
    float paletteX=0, paletteY=0, paletteStep=34, paletteSize=26;
    int paletteCols=7;
    Rectangle swatch(std::size_t i) const {
        return {paletteX+static_cast<float>(i%static_cast<std::size_t>(paletteCols))*paletteStep,
                paletteY+static_cast<float>(i/static_cast<std::size_t>(paletteCols))*paletteStep,paletteSize,paletteSize};
    }
};
struct PointerEdge { int button=0; bool down=false; Vector2 position{}; bool pan=false, pick=false; };
struct KeyEdge { int key=0; bool control=false,shift=false; int character=0; };

class Editor {
public:
    Document document{example()};
    Layout layout;
    Tool tool=Tool::Pencil;
    Modal modal=Modal::None;
    Pixel foreground{218,178,112,255};
    bool grid=true,mirror=false,closed=false,spaceDown=false;
    float zoom=16;
    std::string name="wayfarer",notice="Your next character starts with a single pixel.";
    float noticeRemaining=5;
    std::uint32_t importSerial=0;

    Editor() { prepare(); fit(); syncTexture(); }
    ~Editor() { if (texture_.id) UnloadTexture(texture_); }
    void queuePointer(int which,bool down,Vector2 position,bool pick=false) {
        if (pointerEdges_.size()<128 && std::isfinite(position.x) && std::isfinite(position.y))
            pointerEdges_.push_back({which,down,position,which==1 || spaceDown,pick});
    }
    void queueKey(int key,bool control,bool shift,int character=0) {
        if (keyEdges_.size()<128) keyEdges_.push_back({key,control,shift,character});
    }
    void notify(std::string message) { notice=std::move(message); noticeRemaining=4; }
    void prepare() {
        const float w=static_cast<float>(GetScreenWidth()),h=static_cast<float>(GetScreenHeight());
        const float side=w>=960 && h>=728 ? 280.0f : 0.0f;
        const float dock=side==0 ? 76.0f : 0.0f;
        layout.workspace={64,124,std::max(80.0f,w-64-side),std::max(80.0f,h-152-dock)};
        layout.inspector=side>0 ? Rectangle{w-side,76,side,h-104} : Rectangle{};
        layout.dock=dock>0 ? Rectangle{64,h-28-dock,w-64,dock} : Rectangle{};
        layout.canvas={layout.workspace.x+layout.workspace.width*0.5f+pan_.x-static_cast<float>(document.bitmap().width)*zoom*0.5f,
                       layout.workspace.y+layout.workspace.height*0.5f+pan_.y-static_cast<float>(document.bitmap().height)*zoom*0.5f,
                       static_cast<float>(document.bitmap().width)*zoom,static_cast<float>(document.bitmap().height)*zoom};
        layout.back={16,20,42,34}; layout.create={w-302,20,64,34};
        layout.open={w-230,20,98,34}; layout.save={w-124,20,108,34};
        layout.undo={80,86,62,28}; layout.redo={148,86,62,28};
        layout.mirror={228,86,70,28}; layout.grid={304,86,54,28}; layout.fit={364,86,46,28};
        layout.minus={layout.workspace.x+layout.workspace.width-128,86,26,28};
        layout.plus={layout.workspace.x+layout.workspace.width-34,86,26,28};
        for (std::size_t i=0;i<layout.tools.size();++i) layout.tools[i]={10,140+static_cast<float>(i)*52,44,44};
        const float sx=layout.inspector.x+18;
        layout.hex=side>0 ? Rectangle{sx+50,482,194,34} : Rectangle{8,h-81,46,44};
        layout.name=side>0 ? Rectangle{sx,660,244,32} : Rectangle{12,h-26,170,24};
        layout.clear=side>0 ? Rectangle{sx,616,116,29} : Rectangle{};
        layout.example=side>0 ? Rectangle{sx+124,616,120,29} : Rectangle{};
        if (side>0) {
            layout.paletteX=sx; layout.paletteY=318; layout.paletteCols=7; layout.paletteStep=36; layout.paletteSize=28;
        } else {
            layout.paletteCols=14;
            layout.paletteStep=std::min(34.0f,(layout.dock.width-20)/14);
            layout.paletteSize=layout.paletteStep-5;
            layout.paletteX=layout.dock.x+(layout.dock.width-layout.paletteStep*14)*0.5f;
            layout.paletteY=layout.dock.y+8;
        }
        layout.help={w-60,h-24,48,20};
        const float dw=std::min(480.0f,w-32),dh=modal==Modal::Help ? 402.0f : 302.0f;
        layout.dialog={(w-dw)*0.5f,(h-dh)*0.5f,dw,dh};
        layout.cancel={layout.dialog.x+28,layout.dialog.y+dh-63,(dw-68)*0.5f,35};
        layout.confirm={layout.dialog.x+40+(dw-68)*0.5f,layout.dialog.y+dh-63,(dw-68)*0.5f,35};
        for (std::size_t i=0;i<layout.sizes.size();++i)
            layout.sizes[i]={layout.dialog.x+28+static_cast<float>(i)*(dw-52)/4,layout.dialog.y+139,(dw-68)/4,43};
    }
    void fit() {
        const float ideal=std::min((layout.workspace.width-80)/static_cast<float>(document.bitmap().width),
                                   (layout.workspace.height-80)/static_cast<float>(document.bitmap().height));
        zoom=Zooms.front();
        for (float value : Zooms) if (value<=ideal) zoom=value;
        pan_={}; prepare();
    }
    void frame() {
        noticeRemaining=std::max(0.0f,noticeRemaining-std::min(GetFrameTime(),0.1f));
        const int width=GetScreenWidth(),height=GetScreenHeight();
        if (width!=lastWidth_ || height!=lastHeight_) {
            lastWidth_=width; lastHeight_=height; prepare(); fit();
        }
        prepare();
#ifndef __EMSCRIPTEN__
        const bool control=IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)||IsKeyDown(KEY_LEFT_SUPER)||IsKeyDown(KEY_RIGHT_SUPER);
        const bool shift=IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT);
        constexpr std::array<int,28> keys{{KEY_A,KEY_B,KEY_E,KEY_F,KEY_I,KEY_L,KEY_R,KEY_G,KEY_M,KEY_H,KEY_F1,KEY_N,KEY_O,KEY_S,
            KEY_Z,KEY_Y,KEY_ZERO,KEY_EQUAL,KEY_MINUS,KEY_DELETE,KEY_BACKSPACE,KEY_ENTER,KEY_ESCAPE,KEY_ONE,KEY_TWO,KEY_THREE,KEY_FOUR,KEY_FIVE}};
        for (int key : keys) if (IsKeyPressed(key)) queueKey(key,control,shift);
        spaceDown=IsKeyDown(KEY_SPACE);
        for (int b=0;b<3;++b) {
            const int rb=b==2 ? MOUSE_BUTTON_RIGHT : (b==1 ? MOUSE_BUTTON_MIDDLE : MOUSE_BUTTON_LEFT);
            if (IsMouseButtonPressed(rb)) queuePointer(b,true,GetMousePosition(),IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT));
            if (IsMouseButtonReleased(rb)) queuePointer(b,false,GetMousePosition());
        }
        if (IsFileDropped()) {
            const FilePathList files=LoadDroppedFiles();
            if (files.count>0) importFile(files.paths[0]);
            UnloadDroppedFiles(files);
        }
#endif
        // Opening a native/browser file picker may synchronously blur the
        // window; consume detached queues so focus cleanup cannot invalidate an iterator.
        std::vector<PointerEdge> pointers;
        pointers.swap(pointerEdges_);
        for (const PointerEdge& edge : pointers) {
            if (edge.down) pointerDown(edge);
            else pointerUp(edge);
        }
#ifndef __EMSCRIPTEN__
        for (int c=GetCharPressed();c!=0;c=GetCharPressed()) editCharacter(c);
#else
        while (GetCharPressed()!=0) {} // Browser text is buffered in key order below.
#endif
        std::vector<KeyEdge> bufferedKeys;
        bufferedKeys.swap(keyEdges_);
        for (const KeyEdge& key : bufferedKeys) {
            if (!key.control && key.character!=0) editCharacter(key.character);
            handleKey(key);
        }
        if (panning_) {
            const Vector2 mouse=GetMousePosition();
            pan_.x+=mouse.x-panLast_.x; pan_.y+=mouse.y-panLast_.y; panLast_=mouse;
            const float limitX=(layout.workspace.width+layout.canvas.width)*0.5f-32;
            const float limitY=(layout.workspace.height+layout.canvas.height)*0.5f-32;
            pan_.x=std::clamp(pan_.x,-limitX,limitX); pan_.y=std::clamp(pan_.y,-limitY,limitY);
            prepare();
        } else if (drawing_) drag(GetMousePosition());
        if (modal==Modal::None && inside(GetMousePosition(),layout.workspace) && !drawing_ && !panning_) {
            const float wheel=GetMouseWheelMove();
            if (wheel!=0) changeZoom(wheel>0?1:-1,GetMousePosition());
        }
        syncTexture(); prepare();
        SetMouseCursor(inside(GetMousePosition(),layout.workspace) && modal==Modal::None ?
            (panning_ || spaceDown ? MOUSE_CURSOR_RESIZE_ALL : MOUSE_CURSOR_CROSSHAIR) : MOUSE_CURSOR_DEFAULT);
        draw();
    }
    void requestClose() { requestLeave(); }
    void loseFocus() { finishStroke(); panning_=false; spaceDown=false; pointerEdges_.clear(); keyEdges_.clear(); }
    void importPng(const unsigned char* bytes,std::size_t size) {
        ++importSerial; finishStroke();
        int width=0,height=0;
        if (!pngDimensions(bytes,size,width,height)) { notify("Invalid PNG. Use a PNG from 1 to 128 pixels per side (max 8 MB)."); return; }
        OwnedImage image{LoadImageFromMemory(".png",bytes,static_cast<int>(size))};
        if (!image.value.data || image.value.width!=width || image.value.height!=height) { notify("That PNG could not be decoded. Your artwork is unchanged."); return; }
        ImageFormat(&image.value,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        if (!image.value.data) { notify("PNG conversion failed. Your artwork is unchanged."); return; }
        Bitmap bitmap(width,height);
        static_assert(sizeof(Pixel)==4,"RGBA pixels must be tightly packed");
        std::memcpy(bitmap.pixels.data(),image.value.data,bitmap.pixels.size()*sizeof(Pixel));
        if (document.dirty()) { pendingImport_=std::move(bitmap); modal=Modal::Import; }
        else load(std::move(bitmap),"imported");
    }
private:
    Texture2D texture_{};
    std::uint64_t textureStamp_=0;
    Vector2 pan_{},panLast_{};
    Point strokeStart_{},lastPixel_{};
    Tool strokeTool_=Tool::Pencil;
    bool drawing_=false,panning_=false,hexFocus_=false,nameFocus_=false,selectText_=false;
    Pixel strokeColor_{};
    std::string input_;
    int newSize_=32,lastWidth_=0,lastHeight_=0;
    std::optional<Bitmap> pendingImport_;
    std::vector<PointerEdge> pointerEdges_;
    std::vector<KeyEdge> keyEdges_;
    Point pixel(Vector2 p) const {
        const auto& bitmap=document.bitmap();
        const float x=std::clamp((p.x-layout.canvas.x)/zoom,0.0f,static_cast<float>(bitmap.width-1));
        const float y=std::clamp((p.y-layout.canvas.y)/zoom,0.0f,static_cast<float>(bitmap.height-1));
        return {static_cast<int>(std::floor(x)),static_cast<int>(std::floor(y))};
    }
    void syncTexture() {
        if (textureStamp_==document.stamp()) return;
        const auto& bitmap=document.bitmap();
        if (texture_.width!=bitmap.width || texture_.height!=bitmap.height || !texture_.id) {
            if (texture_.id) UnloadTexture(texture_);
            const Image image{const_cast<Pixel*>(bitmap.pixels.data()),bitmap.width,bitmap.height,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            texture_=LoadTextureFromImage(image);
            SetTextureFilter(texture_,TEXTURE_FILTER_POINT);
            fit();
        } else UpdateTexture(texture_,bitmap.pixels.data());
        textureStamp_=document.stamp();
    }
    void load(Bitmap bitmap,const std::string& fileName) {
        document.replace(std::move(bitmap)); name=fileName; modal=Modal::None; pendingImport_.reset(); fit();
        notify("Sprite ready. Paint, experiment, make it yours.");
    }
    void changeZoom(int direction,Vector2 anchor) {
        const float old=zoom;
        if (direction>0) { for (float level : Zooms) if (level>zoom) { zoom=level; break; } }
        else { for (auto it=Zooms.rbegin();it!=Zooms.rend();++it) if (*it<zoom) { zoom=*it; break; } }
        const float px=(anchor.x-layout.canvas.x)/old,py=(anchor.y-layout.canvas.y)/old;
        prepare();
        pan_.x+=anchor.x-(layout.canvas.x+px*zoom); pan_.y+=anchor.y-(layout.canvas.y+py*zoom); prepare();
    }
    void finishStroke() { if (drawing_) { drawing_=false; document.commitStroke(); } }
    void drag(Vector2 position) {
        if (!drawing_) return;
        const Point next=pixel(position);
        if (strokeTool_==Tool::Pencil || strokeTool_==Tool::Eraser) document.paintLine(lastPixel_,next,strokeColor_,mirror);
        lastPixel_=next;
    }
    void pointerUp(const PointerEdge& edge) {
        if ((edge.button==1 || edge.button==0) && panning_) {
            pan_.x+=edge.position.x-panLast_.x; pan_.y+=edge.position.y-panLast_.y;
            panning_=false; prepare();
        }
        if (!drawing_ || (edge.button!=0 && edge.button!=2)) return;
        drag(edge.position);
        if (strokeTool_==Tool::Line) document.paintLine(strokeStart_,lastPixel_,strokeColor_,mirror);
        else if (strokeTool_==Tool::Rectangle) document.rectangle(strokeStart_,lastPixel_,strokeColor_,mirror);
        finishStroke();
    }
    void setColor(Pixel value) { foreground=value; notify(hex(value)+" selected"); }
    void pointerDown(const PointerEdge& edge) {
        const Vector2 p=edge.position;
        if (modal!=Modal::None) { if (edge.button==0) modalClick(p); return; }
        if (edge.button==0 && inside(p,layout.hex)) { finishStroke(); hexFocus_=true; nameFocus_=false; input_=hex(foreground); selectText_=true; return; }
        if (edge.button==0 && inside(p,layout.name)) { finishStroke(); nameFocus_=true; hexFocus_=false; input_=name; selectText_=true; return; }
        commitText();
        if (edge.button==0) {
            if (inside(p,layout.back)) { requestLeave(); return; }
            if (inside(p,layout.create)) { finishStroke(); modal=Modal::New; return; }
            if (inside(p,layout.open)) { open(); return; }
            if (inside(p,layout.save)) { exportPng(); return; }
            if (inside(p,layout.undo)) { finishStroke(); document.undo(); return; }
            if (inside(p,layout.redo)) { finishStroke(); document.redo(); return; }
            if (inside(p,layout.grid)) { grid=!grid; return; }
            if (inside(p,layout.mirror)) { mirror=!mirror; return; }
            if (inside(p,layout.fit)) { fit(); return; }
            if (layout.workspace.width>540 && inside(p,layout.minus)) { changeZoom(-1,{layout.workspace.x+layout.workspace.width*0.5f,layout.workspace.y+layout.workspace.height*0.5f}); return; }
            if (layout.workspace.width>540 && inside(p,layout.plus)) { changeZoom(1,{layout.workspace.x+layout.workspace.width*0.5f,layout.workspace.y+layout.workspace.height*0.5f}); return; }
            if (inside(p,layout.help)) { modal=Modal::Help; return; }
            if (inside(p,layout.clear)) { finishStroke(); document.clear(); return; }
            if (inside(p,layout.example)) {
                finishStroke(); pendingImport_=example();
                if (document.dirty()) modal=Modal::Import;
                else load(std::move(*pendingImport_),"wayfarer");
                return;
            }
            for (std::size_t i=0;i<layout.tools.size();++i) if (inside(p,layout.tools[i])) { tool=static_cast<Tool>(i); return; }
            for (std::size_t i=0;i<Palette.size();++i) if (inside(p,layout.swatch(i))) { setColor(Palette[i]); return; }
            if (layout.inspector.width>0) {
                const float sx=layout.inspector.x+18;
                for (int i=0;i<3;++i) if (inside(p,{sx+static_cast<float>(i)*84,566,76,30})) {
                    finishStroke();
                    if (i==0) document.flipHorizontal(); else if (i==1) document.flipVertical(); else document.rotateClockwise();
                    return;
                }
            }
        }
        if (!inside(p,layout.workspace)) return;
        if (edge.pan) { finishStroke(); panning_=true; panLast_=p; return; }
        if (!inside(p,layout.canvas)) return;
        const Point at=pixel(p);
        if (edge.pick || tool==Tool::Picker) { setColor(document.bitmap().at(at)); if (tool==Tool::Picker) tool=Tool::Pencil; return; }
        if (edge.button!=0 && edge.button!=2) return;
        strokeTool_=edge.button==2 ? Tool::Eraser : tool;
        strokeColor_=strokeTool_==Tool::Eraser ? Transparent : foreground;
        if (strokeTool_==Tool::Fill) { document.floodFill(at,strokeColor_,mirror); return; }
        document.beginStroke(); drawing_=true; strokeStart_=lastPixel_=at;
        if (strokeTool_==Tool::Pencil || strokeTool_==Tool::Eraser) document.paintLine(at,at,strokeColor_,mirror);
    }
    void commitText() {
        if (hexFocus_) {
            const auto parsed=parseHex(input_);
            if (parsed) setColor(*parsed); else notify("Use 6 hex digits, or 8 to include alpha. Color unchanged.");
        } else if (nameFocus_ && !input_.empty()) name=input_;
        hexFocus_=nameFocus_=selectText_=false;
    }
    void editCharacter(int c) {
        if (!hexFocus_ && !nameFocus_) return;
        const bool validHex=(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')||c=='#';
        const bool validName=(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||c=='-';
        if (!(hexFocus_?validHex:validName)) return;
        if (selectText_) { input_.clear(); selectText_=false; }
        if (input_.size()<(hexFocus_?9u:28u)) input_+=static_cast<char>(c);
    }
    void handleKey(KeyEdge key) {
        if (hexFocus_ || nameFocus_) {
            if (key.key==KEY_ESCAPE) { hexFocus_=nameFocus_=false; }
            else if (key.key==KEY_ENTER) commitText();
            else if (key.key==KEY_BACKSPACE) { if (selectText_) input_.clear(); else if (!input_.empty()) input_.pop_back(); selectText_=false; }
            else if (key.control && key.key==KEY_A) selectText_=true;
            return;
        }
        if (modal!=Modal::None) {
            if (key.key==KEY_ESCAPE) { modal=Modal::None; pendingImport_.reset(); }
            else if (key.key==KEY_ENTER) modalClick({layout.confirm.x+2,layout.confirm.y+2});
            return;
        }
        finishStroke();
        if (key.control) {
            if (key.key==KEY_Z) { if (key.shift) document.redo(); else document.undo(); }
            else if (key.key==KEY_Y) document.redo();
            else if (key.key==KEY_S) exportPng();
            else if (key.key==KEY_O) open();
            else if (key.key==KEY_N) modal=Modal::New;
            return;
        }
        switch (key.key) {
            case KEY_B: tool=Tool::Pencil; break; case KEY_E: tool=Tool::Eraser; break;
            case KEY_F: tool=Tool::Fill; break; case KEY_I: tool=Tool::Picker; break;
            case KEY_L: tool=Tool::Line; break; case KEY_R: tool=Tool::Rectangle; break;
            case KEY_G: grid=!grid; break; case KEY_M: mirror=!mirror; break;
            case KEY_H: case KEY_F1: modal=Modal::Help; break; case KEY_ZERO: fit(); break;
            case KEY_EQUAL: changeZoom(1,{layout.workspace.x+layout.workspace.width*0.5f,layout.workspace.y+layout.workspace.height*0.5f}); break;
            case KEY_MINUS: changeZoom(-1,{layout.workspace.x+layout.workspace.width*0.5f,layout.workspace.y+layout.workspace.height*0.5f}); break;
            case KEY_DELETE: document.clear(); break;
            default: if (key.key>=KEY_ONE && key.key<=KEY_FIVE) setColor(Palette[static_cast<std::size_t>(key.key-KEY_ONE+8)]); break;
        }
    }
    void requestLeave() { finishStroke(); if (document.dirty()) modal=Modal::Leave; else leave(); }
    void leave() {
#ifdef __EMSCRIPTEN__
        EM_ASM({ Module.allowEditorExit=true; window.location.href='index.html'; },0);
#else
        closed=true;
#endif
    }
    void open() {
        finishStroke();
#ifdef __EMSCRIPTEN__
        EM_ASM({ document.getElementById('png-file').click(); },0);
#else
        modal=Modal::OpenHint;
#endif
    }
#ifndef __EMSCRIPTEN__
    void importFile(const char* file) {
        const int length=GetFileLength(file);
        if (length<=0 || static_cast<std::size_t>(length)>MaxImportBytes) { notify("PNG files must be smaller than 8 MB."); return; }
        int size=0; unsigned char* data=LoadFileData(file,&size);
        if (data) importPng(data,static_cast<std::size_t>(std::max(0,size)));
        else notify("Could not open that PNG.");
        UnloadFileData(data);
    }
#endif
    void exportPng(bool overwrite=false) {
        finishStroke(); commitText();
        const std::string file=(name.empty()?"sprite":name)+".png";
#ifndef __EMSCRIPTEN__
        const std::filesystem::path directory="build/sprites";
        std::error_code error;
        std::filesystem::create_directories(directory,error);
        if (error) { notify("Could not create build/sprites. Check folder permissions."); return; }
        const std::string path=(directory/file).string();
        if (!overwrite && FileExists(path.c_str())) { modal=Modal::Overwrite; return; }
#else
        static_cast<void>(overwrite);
#endif
        const auto& bitmap=document.bitmap();
        const Image image{const_cast<Pixel*>(bitmap.pixels.data()),bitmap.width,bitmap.height,1,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
        int count=0;
        std::unique_ptr<unsigned char,void(*)(void*)> bytes{ExportImageToMemory(image,".png",&count),MemFree};
        if (!bytes || count<=0) { notify("PNG encoding failed. Your artwork is still here."); return; }
#ifdef __EMSCRIPTEN__
        downloadSpritePng(bytes.get(),count,file.c_str());
        notify("PNG download started. Export is at original resolution, with transparency.");
#else
        if (!SaveFileData(path.c_str(),bytes.get(),count)) { notify("Could not save the PNG. Your artwork is still here."); return; }
        notify("Saved "+path);
#endif
        document.markSaved(); modal=Modal::None;
    }
    void modalClick(Vector2 p) {
        if (inside(p,layout.cancel)) { modal=Modal::None; pendingImport_.reset(); return; }
        if (modal==Modal::New) {
            constexpr std::array<int,4> sizes{{16,32,64,128}};
            for (std::size_t i=0;i<sizes.size();++i) if (inside(p,layout.sizes[i])) newSize_=sizes[i];
        }
        if (!inside(p,layout.confirm)) return;
        if (modal==Modal::New) load(Bitmap{newSize_,newSize_},"untitled");
        else if (modal==Modal::Import && pendingImport_) load(std::move(*pendingImport_),"imported");
        else if (modal==Modal::Leave) leave();
        else if (modal==Modal::Overwrite) exportPng(true);
        else modal=Modal::None;
    }
    void drawCanvas() const {
        const Rectangle r=layout.workspace,canvas=layout.canvas;
        BeginScissorMode(static_cast<int>(r.x),static_cast<int>(r.y),static_cast<int>(r.width),static_cast<int>(r.height));
        DrawRectangleRec(r,Background);
        for (float x=r.x+16;x<r.x+r.width;x+=32) for (float y=r.y+16;y<r.y+r.height;y+=32)
            DrawPixel(static_cast<int>(x),static_cast<int>(y),Color{42,53,57,150});
        DrawRectangleRec({canvas.x+6,canvas.y+6,canvas.width,canvas.height},Color{0,0,0,50});
        checker(canvas,zoom*2);
        DrawTexturePro(texture_,{0,0,static_cast<float>(texture_.width),static_cast<float>(texture_.height)},canvas,{0,0},0,WHITE);
        if (grid && zoom>=6) {
            for (int x=0;x<=document.bitmap().width;++x) {
                const float px=canvas.x+static_cast<float>(x)*zoom;
                DrawLineEx({px,canvas.y},{px,canvas.y+canvas.height},1,Color{16,23,27,static_cast<unsigned char>(x%8==0?130:70)});
            }
            for (int y=0;y<=document.bitmap().height;++y) {
                const float py=canvas.y+static_cast<float>(y)*zoom;
                DrawLineEx({canvas.x,py},{canvas.x+canvas.width,py},1,Color{16,23,27,static_cast<unsigned char>(y%8==0?130:70)});
            }
        }
        DrawRectangleLinesEx(canvas,1,Border);
        for (int x=0;x<=document.bitmap().width;x+=8) {
            const std::string label=std::to_string(x);
            text(label.c_str(),canvas.x+static_cast<float>(x)*zoom-3,canvas.y-19,9,Muted);
        }
        for (int y=0;y<=document.bitmap().height;y+=8) {
            const std::string label=std::to_string(y);
            text(label.c_str(),canvas.x-25,canvas.y+static_cast<float>(y)*zoom-4,9,Muted);
        }
        if (modal==Modal::None && inside(GetMousePosition(),r) && (inside(GetMousePosition(),canvas)||drawing_) && !panning_ && !spaceDown) {
            const auto paintPreview=[&](Point point,Pixel tint) {
                const Rectangle cell{canvas.x+static_cast<float>(point.x)*zoom,canvas.y+static_cast<float>(point.y)*zoom,zoom,zoom};
                DrawRectangleRec(cell,fade(color(tint),0.55f)); DrawRectangleLinesEx(cell,1.5f,fade(Ink,0.85f));
                if (mirror) {
                    const float mx=canvas.x+static_cast<float>(document.bitmap().width-1-point.x)*zoom;
                    DrawRectangleLinesEx({mx,cell.y,zoom,zoom},1,fade(Gold,0.6f));
                }
            };
            if (drawing_ && (strokeTool_==Tool::Line || strokeTool_==Tool::Rectangle)) {
                if (strokeTool_==Tool::Line) for (Point p : linePoints(strokeStart_,lastPixel_)) paintPreview(p,strokeColor_);
                else {
                    for (Point p : linePoints(strokeStart_,{lastPixel_.x,strokeStart_.y})) paintPreview(p,strokeColor_);
                    for (Point p : linePoints({lastPixel_.x,strokeStart_.y},lastPixel_)) paintPreview(p,strokeColor_);
                    for (Point p : linePoints(lastPixel_,{strokeStart_.x,lastPixel_.y})) paintPreview(p,strokeColor_);
                    for (Point p : linePoints({strokeStart_.x,lastPixel_.y},strokeStart_)) paintPreview(p,strokeColor_);
                }
            } else paintPreview(pixel(GetMousePosition()),tool==Tool::Eraser ? Transparent : foreground);
        }
        if (mirror) DrawLineEx({canvas.x+canvas.width*0.5f,canvas.y},{canvas.x+canvas.width*0.5f,canvas.y+canvas.height},1,fade(Gold,0.55f));
        EndScissorMode();
    }
    void drawPalette() const {
        for (std::size_t i=0;i<Palette.size();++i) {
            const Rectangle r=layout.swatch(i);
            if (Palette[i].a<255) checker(r,5); else DrawRectangleRec(r,color(Palette[i]));
            if (Palette[i]==foreground || inside(GetMousePosition(),r)) {
                DrawRectangleLinesEx({r.x-2,r.y-2,r.width+4,r.height+4},1,Palette[i]==foreground?Gold:Muted);
                if (Palette[i]==foreground) DrawRectangleRec({r.x+r.width-5,r.y+r.height-5,3,3},Ink);
            }
        }
    }
    void drawInspector() const {
        const Rectangle r=layout.inspector;
        if (r.width<=0) {
            DrawRectangleRec(layout.dock,Panel); divider(layout.dock.x,layout.dock.y,layout.dock.width); drawPalette(); return;
        }
        DrawRectangleRec(r,Panel); DrawLineEx({r.x,r.y},{r.x,r.y+r.height},1,Border);
        BeginScissorMode(static_cast<int>(r.x),static_cast<int>(r.y),static_cast<int>(r.width),static_cast<int>(r.height));
        const float sx=r.x+18;
        text("LIVE PREVIEW",sx,100,10,Muted);
        text("NEAREST NEIGHBOR",sx+126,101,8,Muted);
        const Rectangle preview{sx,126,244,146}; checker(preview,10);
        const float scale=std::min(4.0f,std::min((preview.width-20)/static_cast<float>(texture_.width),(preview.height-16)/static_cast<float>(texture_.height)));
        const float pw=static_cast<float>(texture_.width)*scale,ph=static_cast<float>(texture_.height)*scale;
        DrawTexturePro(texture_,{0,0,static_cast<float>(texture_.width),static_cast<float>(texture_.height)},
            {preview.x+(preview.width-pw)*0.5f,preview.y+(preview.height-ph)*0.5f,pw,ph},{0,0},0,WHITE);
        DrawRectangleLinesEx(preview,1,Border);
        text("PALETTE",sx,294,10,Muted); text("28 COLORS",sx+181,295,9,Muted); drawPalette();
        text("FOREGROUND / RGBA",sx,462,9,Muted);
        checker({sx,482,36,34},7); DrawRectangleRec({sx,482,36,34},color(foreground)); DrawRectangleLinesEx({sx,482,36,34},1,Border);
        const auto field=[&](Rectangle box,const std::string& value,bool focus) {
            DrawRectangleRec(box,Background); DrawRectangleLinesEx(box,1,focus?Gold:Border);
            text(value.c_str(),box.x+10,box.y+11,11,focus?Gold:Ink);
            if (focus && std::fmod(GetTime(),1.0)<0.5) {
                const float cursor=box.x+10+measure(value.c_str(),11)+2;
                DrawLineEx({cursor,box.y+8},{cursor,box.y+25},1,Gold);
            }
        };
        field(layout.hex,hexFocus_?input_:hex(foreground),hexFocus_);
        text("6 HEX DIGITS + OPTIONAL 2-DIGIT ALPHA",sx,526,8,Muted);
        divider(sx,548,244); text("TRANSFORM",sx,550,8,Muted);
        for (int i=0;i<3;++i) button({sx+static_cast<float>(i)*84,566,76,30},i==0?"FLIP H":(i==1?"FLIP V":"ROTATE"));
        button(layout.clear,"CLEAR"); button(layout.example,"EXAMPLE");
        text("FILE NAME",sx,648,8,Muted); field(layout.name,nameFocus_?input_:name,nameFocus_);
        text(".png",sx+210,671,10,Muted);
        if (r.height>688) {
            divider(sx,724,244); text("SMALL CANVAS. BIG POSSIBILITIES.",sx,744,9,Teal);
            text("PNG exports keep original pixels + alpha.",sx,766,9,Muted);
            text("No uploads. No external assets required.",sx,783,9,Muted);
        }
        EndScissorMode();
    }
    void drawModal() const {
        if (modal==Modal::None) return;
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{6,10,13,220});
        const Rectangle r=layout.dialog;
        DrawRectangleRec(r,Panel); DrawRectangleLinesEx(r,1,Border); divider(r.x,r.y,r.width);
        DrawRectangleRec({r.x,r.y,48,2},Gold);
        const char* title="NEW SPRITE";
        if (modal==Modal::Import) title="REPLACE UNSAVED WORK?";
        else if (modal==Modal::Leave) title="LEAVE THE WORKSHOP?";
        else if (modal==Modal::Help) title="THE PIXEL PLAYBOOK";
        else if (modal==Modal::Overwrite) title="REPLACE EXPORTED PNG?";
        else if (modal==Modal::OpenHint) title="IMPORT A PNG";
        text("ASHFALL / SPRITE WORKSHOP",r.x+28,r.y+28,9,Gold); text(title,r.x+28,r.y+58,20,Ink);
        if (modal==Modal::New) {
            text("Choose a canvas. Start with transparent pixels.",r.x+28,r.y+99,11,Muted);
            constexpr std::array<int,4> sizes{{16,32,64,128}};
            for (std::size_t i=0;i<sizes.size();++i) {
                const std::string label=std::to_string(sizes[i])+" x "+std::to_string(sizes[i]);
                button(layout.sizes[i],label.c_str(),newSize_==sizes[i]);
            }
            text(document.dirty()?"Unsaved changes will be discarded. Export them first.":"Small sprites. Sharp edges. Every pixel counts.",r.x+28,r.y+202,10,document.dirty()?Gold:Muted);
        } else if (modal==Modal::Help) {
            constexpr std::array<const char*,7> rows{{"B  Pencil     E  Eraser     F  Flood fill", "I  Picker     L  Line       R  Rectangle",
                "G  Grid       M  Mirror     0  Fit canvas", "Ctrl/Cmd Z  Undo       Shift Z / Y  Redo",
                "Ctrl/Cmd N  New    O  Open    S  Export", "Wheel / +/-  Zoom    Space + drag  Pan", "Right drag  Erase    Alt-click  Pick color"}};
            for (std::size_t i=0;i<rows.size();++i) text(rows[i],r.x+28,r.y+106+static_cast<float>(i)*28,11,Muted);
        } else if (modal==Modal::OpenHint) {
            text("Drop a PNG file anywhere on this window.",r.x+28,r.y+112,12,Ink);
            text("1-128 pixels per side. Transparency is preserved.",r.x+28,r.y+145,10,Muted);
            text("Exports are saved to build/sprites/<name>.png.",r.x+28,r.y+175,10,Muted);
        } else {
            text(modal==Modal::Overwrite?"A PNG with this file name already exists.":"You have changes that have not been exported.",r.x+28,r.y+117,12,Muted);
            text(modal==Modal::Import?"Importing replaces the canvas and clears undo history.":
                 (modal==Modal::Overwrite?"Replace the file with your current sprite?":"Keep editing to export a PNG before leaving."),r.x+28,r.y+153,10,Muted);
        }
        button(layout.cancel,modal==Modal::Help || modal==Modal::OpenHint ? "CLOSE":"KEEP EDITING");
        const char* confirm=modal==Modal::New?"CREATE SPRITE":(modal==Modal::Import?"IMPORT PNG":
            (modal==Modal::Leave?"LEAVE":(modal==Modal::Overwrite?"REPLACE PNG":"GOT IT")));
        button(layout.confirm,confirm,false,false,true);
    }
    void draw() const {
        BeginDrawing(); ClearBackground(Background); drawCanvas();
        DrawRectangleRec({0,0,static_cast<float>(GetScreenWidth()),76},Panel); divider(0,75,static_cast<float>(GetScreenWidth()));
        button(layout.back,"<");
        text("ASHFALL / TOOLS",80,18,9,Gold);
        text(GetScreenWidth()<900?"Sprites":"Sprite Workshop",80,34,22,Ink);
        if (GetScreenWidth()>=1120) text("A LITTLE WORKSHOP FOR YOUR NEXT ADVENTURE",342,34,9,Muted);
        button(layout.create,"NEW"); button(layout.open,"OPEN PNG"); button(layout.save,"EXPORT PNG",false,false,true);
        DrawRectangleRec({64,76,layout.workspace.width,48},Panel); divider(64,123,layout.workspace.width);
        button(layout.undo,"UNDO",false,document.undoDepth()==0 && !document.editing());
        button(layout.redo,"REDO",false,document.redoDepth()==0);
        button(layout.mirror,"MIRROR",mirror); button(layout.grid,"GRID",grid); button(layout.fit,"FIT");
        if (layout.workspace.width>540) {
            button(layout.minus,"-"); button(layout.plus,"+");
            char percent[20]; std::snprintf(percent,sizeof(percent),"%.0f%%",static_cast<double>(zoom*100));
            centered(percent,{layout.minus.x+28,86,64,28},10,Muted);
        }
        DrawRectangleRec({0,76,64,static_cast<float>(GetScreenHeight()-104)},Panel);
        DrawLineEx({63,76},{63,static_cast<float>(GetScreenHeight()-28)},1,Border);
        constexpr std::array<const char*,6> letters{{"B","E","F","I","L","R"}};
        constexpr std::array<const char*,6> names{{"PENCIL","ERASER","FLOOD FILL","COLOR PICKER","LINE","RECTANGLE"}};
        for (std::size_t i=0;i<layout.tools.size();++i) {
            const Rectangle r=layout.tools[i]; const bool active=static_cast<int>(tool)==static_cast<int>(i);
            const bool hover=inside(GetMousePosition(),r);
            if (active || hover) DrawRectangleRec(r,active?Color{57,52,40,255}:Raised);
            if (active) DrawRectangleRec({0,r.y+9,2,26},Gold);
            toolIcon(static_cast<int>(i),{r.x+22,r.y+20},active?Gold:Muted);
            text(letters[i],r.x+33,r.y+33,8,active?Gold:Muted);
            if (hover && modal==Modal::None) {
                const float width=measure(names[i],10)+24;
                DrawRectangleRec({r.x+r.width+8,r.y+12,width,25},Raised);
                DrawRectangleLinesEx({r.x+r.width+8,r.y+12,width,25},1,Border);
                text(names[i],r.x+r.width+20,r.y+20,10,Ink);
            }
        }
        const float colorY=static_cast<float>(GetScreenHeight())-77;
        checker({20,colorY+12,25,25},5); DrawRectangleLinesEx({20,colorY+12,25,25},1,Border);
        DrawRectangleRec({12,colorY+4,25,25},color(foreground)); DrawRectangleLinesEx({12,colorY+4,25,25},1,Gold);
        drawInspector();
        const float footer=static_cast<float>(GetScreenHeight())-28;
        DrawRectangleRec({0,footer,static_cast<float>(GetScreenWidth()),28},Panel); divider(0,footer,static_cast<float>(GetScreenWidth()));
        const std::string filename=(document.dirty()?"* ":"")+(nameFocus_?input_:name)+".png";
        text(filename.c_str(),16,footer+10,10,document.dirty()?Gold:Ink);
        char dimensions[44]; std::snprintf(dimensions,sizeof(dimensions),"%d x %d PX  /  RGBA",document.bitmap().width,document.bitmap().height);
        text(dimensions,200,footer+10,9,Muted);
        if (GetScreenWidth()>=1240 && noticeRemaining>0) text(notice.c_str(),390,footer+10,9,Teal);
        button(layout.help,"? HELP");
        if (hexFocus_ && layout.inspector.width==0) {
            const Rectangle field{76,static_cast<float>(GetScreenHeight())-146,194,40};
            DrawRectangleRec(field,Raised); DrawRectangleLinesEx(field,1,Gold);
            text(input_.c_str(),field.x+12,field.y+14,12,Gold);
        }
        if (GetScreenWidth()<1240 && noticeRemaining>0 && modal==Modal::None && !hexFocus_ && !nameFocus_) {
            const Rectangle toast{layout.workspace.x+12,layout.workspace.y+layout.workspace.height-44,
                layout.workspace.width-24,32};
            DrawRectangleRec(toast,Panel); DrawRectangleLinesEx(toast,1,Border);
            std::string caption=notice;
            const float available=toast.width-24;
            if (measure(caption.c_str(),10)>available) {
                while (!caption.empty() && measure((caption+"...").c_str(),10)>available) caption.pop_back();
                caption+="...";
            }
            text(caption.c_str(),toast.x+12,toast.y+11,10,Teal);
        }
        drawModal(); EndDrawing();
    }
};
} // namespace ashfall::sprite

namespace {
std::unique_ptr<ashfall::sprite::Editor> editor;
void editorFrame() { editor->frame(); }
#ifdef __EMSCRIPTEN__
std::vector<unsigned char> importBytes;
EM_BOOL mouseEdge(int event,const EmscriptenMouseEvent* mouse,void*) {
    if (editor) editor->queuePointer(mouse->button,event==EMSCRIPTEN_EVENT_MOUSEDOWN,
        {static_cast<float>(mouse->targetX),static_cast<float>(mouse->targetY)},mouse->altKey);
    return EM_FALSE;
}
int keyCode(const EmscriptenKeyboardEvent& event) {
    if (std::strcmp(event.code,"F1")==0) return KEY_F1;
    if (std::strcmp(event.code,"Escape")==0) return KEY_ESCAPE;
    if (std::strcmp(event.code,"Enter")==0 || std::strcmp(event.code,"NumpadEnter")==0) return KEY_ENTER;
    if (std::strcmp(event.code,"Backspace")==0) return KEY_BACKSPACE;
    if (std::strcmp(event.code,"Delete")==0) return KEY_DELETE;
    if (std::strcmp(event.code,"Equal")==0) return KEY_EQUAL;
    if (std::strcmp(event.code,"Minus")==0) return KEY_MINUS;
    return static_cast<int>(event.keyCode);
}
EM_BOOL keyEdge(int type,const EmscriptenKeyboardEvent* event,void*) {
    if (!editor) return EM_FALSE;
    const int key=keyCode(*event);
    if (key==KEY_SPACE) editor->spaceDown=type==EMSCRIPTEN_EVENT_KEYDOWN;
    if (type==EMSCRIPTEN_EVENT_KEYDOWN && !event->repeat) {
        const int character=std::strlen(event->key)==1 ? static_cast<unsigned char>(event->key[0]) : 0;
        editor->queueKey(key,event->ctrlKey||event->metaKey,event->shiftKey,character);
    }
    return EM_FALSE;
}
EM_BOOL resized(int,const EmscriptenUiEvent*,void*) {
    double width=0,height=0;
    if (emscripten_get_element_css_size("#canvas",&width,&height)==EMSCRIPTEN_RESULT_SUCCESS)
        SetWindowSize(std::clamp(static_cast<int>(width),1,8192),std::clamp(static_cast<int>(height),1,8192));
    return EM_FALSE;
}
EM_BOOL blurred(int,const EmscriptenFocusEvent*,void*) { if (editor) editor->loseFocus(); return EM_FALSE; }
#endif
}

#ifdef __EMSCRIPTEN__
extern "C" {
EMSCRIPTEN_KEEPALIVE unsigned char* ashfall_editor_import_buffer(int size) {
    if (size<=0 || static_cast<std::size_t>(size)>ashfall::sprite::MaxImportBytes) return nullptr;
    importBytes.resize(static_cast<std::size_t>(size)); return importBytes.data();
}
EMSCRIPTEN_KEEPALIVE void ashfall_editor_import_png(int size) {
    if (editor && size>0 && static_cast<std::size_t>(size)==importBytes.size()) editor->importPng(importBytes.data(),importBytes.size());
    else if (editor) editor->importPng(nullptr,0);
    importBytes.clear();
}
EMSCRIPTEN_KEEPALIVE int ashfall_editor_is_dirty() { return editor && editor->document.dirty(); }
}
EM_JS(void,installBrowserFiles,(),{
    const input=document.getElementById('png-file');
    async function readFile(file){
        if(!file) return;
        if(file.size===0 || file.size>8388608){_ashfall_editor_import_png(0);return;}
        try {
            const bytes=new Uint8Array(await file.arrayBuffer());
            const pointer=_ashfall_editor_import_buffer(bytes.length);
            if(!pointer){_ashfall_editor_import_png(0);return;}
            HEAPU8.set(bytes,pointer); _ashfall_editor_import_png(bytes.length);
        } catch(error) {_ashfall_editor_import_png(0);}
        Module.canvas.focus({preventScroll:true});
    }
    input.addEventListener('change',function(){const file=input.files[0];input.value="";readFile(file);});
    Module.canvas.addEventListener('dragover',function(event){event.preventDefault();});
    Module.canvas.addEventListener('drop',function(event){event.preventDefault();readFile(event.dataTransfer.files[0]);});
    window.addEventListener('beforeunload',function(event){
        if(!Module.allowEditorExit && _ashfall_editor_is_dirty()){event.preventDefault();event.returnValue="";}
    });
});
#endif

#if defined(__EMSCRIPTEN__) && defined(ASHFALL_WEB_TEST_API)
extern "C" EMSCRIPTEN_KEEPALIVE const char* ashfall_editor_get_state() {
    static char state[4096];
    const auto& doc=editor->document; const auto& r=editor->layout;
    std::snprintf(state,sizeof(state),
        "{\"width\":%d,\"height\":%d,\"dirty\":%s,\"undo\":%zu,\"redo\":%zu,\"stamp\":%llu,\"importSerial\":%u,"
        "\"foreground\":%u,\"tool\":%d,\"modal\":%d,\"grid\":%s,\"mirror\":%s,\"zoom\":%.2f,"
        "\"canvas\":{\"x\":%.2f,\"y\":%.2f,\"width\":%.2f,\"height\":%.2f},"
        "\"palette\":{\"x\":%.2f,\"y\":%.2f,\"step\":%.2f,\"size\":%.2f,\"cols\":%d},"
        "\"dialog\":{\"x\":%.2f,\"y\":%.2f},\"confirm\":{\"x\":%.2f,\"y\":%.2f},"
        "\"hex\":{\"x\":%.2f,\"y\":%.2f},\"name\":{\"x\":%.2f,\"y\":%.2f}}",
        doc.bitmap().width,doc.bitmap().height,doc.dirty()?"true":"false",doc.undoDepth(),doc.redoDepth(),
        static_cast<unsigned long long>(doc.stamp()),editor->importSerial,editor->foreground.rgba(),static_cast<int>(editor->tool),static_cast<int>(editor->modal),
        editor->grid?"true":"false",editor->mirror?"true":"false",static_cast<double>(editor->zoom),
        static_cast<double>(r.canvas.x),static_cast<double>(r.canvas.y),static_cast<double>(r.canvas.width),static_cast<double>(r.canvas.height),
        static_cast<double>(r.paletteX),static_cast<double>(r.paletteY),static_cast<double>(r.paletteStep),static_cast<double>(r.paletteSize),r.paletteCols,
        static_cast<double>(r.dialog.x),static_cast<double>(r.dialog.y),static_cast<double>(r.confirm.x),static_cast<double>(r.confirm.y),
        static_cast<double>(r.hex.x),static_cast<double>(r.hex.y),static_cast<double>(r.name.x),static_cast<double>(r.name.y));
    return state;
}
extern "C" EMSCRIPTEN_KEEPALIVE unsigned int ashfall_editor_pixel(int x,int y) { return editor->document.bitmap().at({x,y}).rgba(); }
#endif

int main() {
    SetTraceLogLevel(LOG_ERROR); SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    int width=1440,height=900;
#ifdef __EMSCRIPTEN__
    width=EM_ASM_INT({return window.innerWidth;},0); height=EM_ASM_INT({return window.innerHeight;},0);
#endif
    InitWindow(width,height,"Ashfall | Sprite Workshop");
    if (!IsWindowReady()) return 1;
    SetExitKey(KEY_NULL); SetTextureFilter(GetFontDefault().texture,TEXTURE_FILTER_POINT);
    editor=std::make_unique<ashfall::sprite::Editor>();
#ifdef __EMSCRIPTEN__
    installBrowserFiles();
    emscripten_set_mousedown_callback("#canvas",nullptr,EM_TRUE,mouseEdge);
    emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_TRUE,mouseEdge);
    emscripten_set_keydown_callback("#canvas",nullptr,EM_TRUE,keyEdge);
    emscripten_set_keyup_callback("#canvas",nullptr,EM_TRUE,keyEdge);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_FALSE,resized);
    emscripten_set_blur_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_FALSE,blurred);
    emscripten_set_main_loop(editorFrame,0,EM_TRUE);
#else
    SetWindowMinSize(800,640); SetTargetFPS(60);
    while (!editor->closed) {
        if (WindowShouldClose()) editor->requestClose();
        if (!IsWindowFocused()) editor->loseFocus();
        if (!editor->closed) editorFrame();
    }
    editor.reset(); CloseWindow();
#endif
    return 0;
}

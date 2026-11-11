#include "renderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

namespace ashfall {
namespace {
constexpr Color Background{12, 16, 18, 255};
constexpr Color Panel{18, 23, 25, 255};
constexpr Color Raised{25, 31, 32, 255};
constexpr Color Border{45, 53, 53, 255};
constexpr Color Ink{222, 224, 208, 255};
constexpr Color Muted{126, 140, 135, 255};
constexpr Color Dim{76, 91, 88, 255};
constexpr Color Gold{211, 171, 104, 255};
constexpr Color Teal{104, 177, 157, 255};
constexpr Color Red{200, 78, 65, 255};
Vector2 rv(Vec2 p) { return {p.x, p.y}; }
Vector2 add(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
Color tint(Rgb c) { return {c.r, c.g, c.b, 255}; }
Color alpha(Color c, float opacity) {
    c.a = static_cast<unsigned char>(std::clamp(opacity, 0.0f, 1.0f) * static_cast<float>(c.a));
    return c;
}
Color shade(Color c, float factor) {
    return {static_cast<unsigned char>(std::clamp(static_cast<float>(c.r) * factor, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(static_cast<float>(c.g) * factor, 0.0f, 255.0f)),
            static_cast<unsigned char>(std::clamp(static_cast<float>(c.b) * factor, 0.0f, 255.0f)), c.a};
}
bool inside(Vector2 p, Rectangle r) { return r.width > 0 && CheckCollisionPointRec(p, r); }
void line(Vector2 a, Vector2 b, float width, Color color) { DrawLineEx(a, b, width, color); }
void quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color color) {
    DrawTriangle(a, d, c, color);
    DrawTriangle(a, c, b, color);
}
void polygon(const Vector2* points, std::size_t count, Color color) {
    if (count < 3) return;
    for (std::size_t i = 1; i + 1 < count; ++i) DrawTriangle(points[0], points[i + 1], points[i], color);
}
void polygon(std::initializer_list<Vector2> points, Color color) {
    polygon(points.begin(),points.size(),color);
}
void text(const char* value, float x, float y, float size, Color color = Ink, float spacing = 1.0f) {
    DrawTextEx(GetFontDefault(), value, {x, y}, std::max(10.0f,size), spacing, color);
}
float textWidth(const char* value, float size, float spacing = 1.0f) {
    return MeasureTextEx(GetFontDefault(), value, std::max(10.0f,size), spacing).x;
}
void centered(const char* value, float x, float y, float size, Color color = Ink, float spacing = 1.0f) {
    text(value, x - textWidth(value, size, spacing) * 0.5f, y, size, color, spacing);
}
void horizontal(float x, float y, float width, Color color = Border) { line({x, y}, {x + width, y}, 1, color); }
void diamond(Vector2 p, float width, float height, Color color) {
    quad({p.x, p.y - height}, {p.x + width, p.y}, {p.x, p.y + height}, {p.x - width, p.y}, color);
}
void diamondOutline(Vector2 p, float width, float height, Color color, float thickness = 1) {
    line({p.x, p.y - height}, {p.x + width, p.y}, thickness, color);
    line({p.x + width, p.y}, {p.x, p.y + height}, thickness, color);
    line({p.x, p.y + height}, {p.x - width, p.y}, thickness, color);
    line({p.x - width, p.y}, {p.x, p.y - height}, thickness, color);
}
void prism(Vector2 p, float w, float d, float height, Color top, Color left, Color right) {
    const Vector2 a{p.x, p.y - d - height}, b{p.x + w, p.y - height};
    const Vector2 c{p.x, p.y + d - height}, e{p.x - w, p.y - height};
    quad(e, c, {c.x, c.y + height}, {e.x, e.y + height}, left);
    quad(c, b, {b.x, b.y + height}, {c.x, c.y + height}, right);
    quad(a, b, c, e, top);
    line(e, c, 1, shade(top, 1.12f));
    line(c, b, 1, shade(top, 0.8f));
}
void icon(int kind, Vector2 c, float s, Color color) {
    auto p = [c, s](float x, float y) { return Vector2{c.x + x * s, c.y + y * s}; };
    const float thick = std::max(1.0f, s * 0.085f);
    if (kind == 0) { // sword
        line(p(-0.38f, 0.4f), p(0.4f, -0.4f), thick * 1.5f, color);
        line(p(-0.32f, 0.02f), p(0.02f, 0.35f), thick, color);
        line(p(0.4f, -0.4f), p(0.39f, -0.15f), thick, color);
        line(p(0.4f, -0.4f), p(0.15f, -0.39f), thick, color);
    } else if (kind == 1) { // armor
        polygon({p(-0.42f,-0.4f), p(-0.13f,-0.49f), p(0,-0.27f), p(0.13f,-0.49f),
                 p(0.42f,-0.4f), p(0.32f,-0.1f), p(0.23f,0), p(0.26f,0.43f),
                 p(-0.26f,0.43f), p(-0.23f,0), p(-0.32f,-0.1f)}, alpha(color, 0.8f));
        line(p(0,-0.14f), p(0,0.3f), thick, shade(color, 1.35f));
    } else if (kind == 2) { // boots
        polygon({p(-0.3f,-0.45f),p(0.15f,-0.45f),p(0.13f,0.1f),p(0.45f,0.24f),
                 p(0.45f,0.43f),p(-0.33f,0.43f)}, alpha(color, 0.9f));
        line(p(-0.18f,-0.27f),p(0.07f,-0.27f),thick,shade(color,1.35f));
    } else if (kind == 3) { // hood
        polygon({p(0,-0.53f),p(0.43f,-0.12f),p(0.38f,0.4f),p(-0.38f,0.4f),p(-0.43f,-0.12f)}, color);
        diamond(p(0,0.04f),s*0.20f,s*0.27f,Panel);
        line(p(-0.09f,0.02f),p(0.09f,0.02f),thick,Gold);
    } else if (kind == 4) { // flask
        DrawCircleV(p(0,0.14f), s*0.32f, alpha(color,0.8f));
        DrawRectangleRec({c.x-s*0.13f,c.y-s*0.42f,s*0.26f,s*0.45f},color);
        line(p(-0.21f,-0.44f),p(0.21f,-0.44f),thick*1.3f,Gold);
        line(p(-0.12f,0.06f),p(-0.12f,0.25f),thick,alpha(Ink,0.5f));
    } else if (kind == 5) { // dash
        for (int i = 0; i < 3; ++i) {
            const float dx = -0.45f + static_cast<float>(i)*0.32f;
            line(p(dx,-0.32f),p(dx+0.24f,0),thick,color);
            line(p(dx+0.24f,0),p(dx,0.32f),thick,color);
        }
    } else if (kind == 6) { // pause
        line(p(-0.17f,-0.32f),p(-0.17f,0.32f),thick*1.5f,color);
        line(p(0.17f,-0.32f),p(0.17f,0.32f),thick*1.5f,color);
    } else if (kind == 7) { // compass
        diamondOutline(c,s*0.43f,s*0.5f,color);
        line(p(0,-0.5f),p(0,0.5f),thick,color);
        line(p(-0.43f,0),p(0.43f,0),thick,color);
    } else if (kind == 8) { // flame
        polygon({p(-0.34f,0.35f),p(-0.4f,-0.05f),p(-0.13f,-0.38f),p(-0.09f,-0.09f),
                 p(0.15f,-0.65f),p(0.4f,-0.02f),p(0.29f,0.39f),p(0,0.53f)},color);
        polygon({p(-0.14f,0.32f),p(0.09f,-0.15f),p(0.2f,0.30f),p(0,0.47f)},Panel);
    } else if (kind == 9) { // fullscreen
        for (float x : {-1.0f,1.0f}) for (float y : {-1.0f,1.0f}) {
            line(p(x*0.4f,y*0.18f),p(x*0.4f,y*0.4f),thick,color);
            line(p(x*0.4f,y*0.4f),p(x*0.18f,y*0.4f),thick,color);
        }
    }
}
void button(Rectangle r, const char* label, bool primary = false) {
    const bool hover = inside(GetMousePosition(),r);
    DrawRectangleRec(r,primary ? (hover ? shade(Gold,1.13f) : Gold) : (hover ? Raised : Panel));
    DrawRectangleLinesEx(r,1,primary ? Gold : Border);
    centered(label,r.x+r.width*0.5f,r.y+(r.height-12)*0.5f,12,primary ? Background : Ink,1.2f);
}
void keycap(const char* value, float x, float y, float width) {
    DrawRectangleRec({x,y,width,21},Raised);
    DrawRectangleLinesEx({x,y,width,21},1,Border);
    centered(value,x+width*0.5f,y+5,10,Ink);
}
}

void Renderer::prepare(const World& world, const ViewState& view, float frameDt) {
    time_ += frameDt;
    const float w = static_cast<float>(GetScreenWidth()), h = static_cast<float>(GetScreenHeight());
    const float head = w < 760 ? 64.0f : 80.0f;
    const float foot = w < 760 ? 100.0f : 110.0f;
    const float side = w < 1100 ? 266.0f : 300.0f;
    layout_.sidebarVisible = view.inventory;
    const bool overlay = w < 960;
    const float sceneWidth = w - (view.inventory && !overlay ? side : 0);
    layout_.header = {0,0,w,head};
    layout_.scene = {0,head,sceneWidth,std::max(80.0f,h-head-foot)};
    layout_.footer = {0,h-foot,sceneWidth,foot};
    layout_.sidebar = view.inventory ? Rectangle{w-side,head,side,h-head} : Rectangle{};
    const float tabX = w >= 1100 ? 330.0f : (w >= 760 ? 275.0f : 170.0f);
    layout_.inventoryButton = {tabX,0,w < 760 ? 82.0f : 115.0f,head};
    layout_.helpButton = {tabX+(w < 760 ? 82.0f : 115.0f),0,w < 760 ? 60.0f : 110.0f,head};
    layout_.fullscreenButton = {w-48,head*0.5f-18,36,36};
    layout_.pauseButton = {sceneWidth-60,h-foot+34,36,36};
    if (w < 760) layout_.pauseButton = {sceneWidth-40,h-foot+25,30,30};
    const float sx = layout_.sidebar.x+22;
    layout_.headSlot = {sx,head+254,side-44,68};
    layout_.armorSlot = {sx,head+330,side-44,68};
    layout_.bootsSlot = {sx,head+406,side-44,68};
    const float abilitiesX = sceneWidth < 760 ? sceneWidth*0.44f : std::max(330.0f,sceneWidth*0.48f);
    layout_.attackButton = {abilitiesX,h-foot+24,52,52};
    layout_.healButton = {abilitiesX+62,h-foot+24,52,52};
    layout_.dashButton = {abilitiesX+124,h-foot+24,52,52};
    if (sceneWidth < 600) {
        layout_.attackButton.width = layout_.healButton.width = layout_.dashButton.width = 40;
        layout_.attackButton.height = layout_.healButton.height = layout_.dashButton.height = 44;
        layout_.healButton.x = abilitiesX+46;
        layout_.dashButton.x = abilitiesX+92;
    }
    layout_.zoomOut = {sceneWidth-126,h-foot-54,30,30};
    layout_.resetZoom = {sceneWidth-90,h-foot-54,30,30};
    layout_.zoomIn = {sceneWidth-54,h-foot-54,30,30};
    const float modalWidth = std::min(520.0f,w-32);
    const float modalHeight = view.help ? std::min(470.0f,h-30) : 302.0f;
    layout_.modal = {(w-modalWidth)*0.5f,(h-modalHeight)*0.5f,modalWidth,modalHeight};
    layout_.modalPrimary = {layout_.modal.x+36,layout_.modal.y+modalHeight-71,modalWidth-72,39};
    layout_.modalSecondary = {layout_.modal.x+36,layout_.modal.y+modalHeight-113,modalWidth-72,30};
    camera_.target = rv(iso_.worldToScreen({10,10}));
    camera_.offset = {sceneWidth*0.5f,head+layout_.scene.height*0.57f};
    camera_.rotation = 0;
    camera_.zoom = std::max(0.2f,std::min((sceneWidth-46)/1170.0f,
        std::max(150.0f,layout_.scene.height-114)/576.0f)) * view.zoom;
    camera_.offset.x += std::sin(time_*139.0f)*world.screenShake*4.0f;
    camera_.offset.y += std::cos(time_*157.0f)*world.screenShake*3.0f;
}
Vec2 Renderer::screenToWorld(Vector2 p) const {
    const Vector2 plane = GetScreenToWorld2D(p,camera_);
    return iso_.screenToWorld({plane.x,plane.y});
}
Vector2 Renderer::worldToScreen(Vec2 p) const { return GetWorldToScreen2D(rv(iso_.worldToScreen(p)),camera_); }
bool Renderer::sceneContains(Vector2 mouse, const World& world, const ViewState& view) const {
    return !view.paused && !view.help && world.player.alive() && !world.cleared() &&
           inside(mouse,layout_.scene) && !(layout_.sidebarVisible && inside(mouse,layout_.sidebar)) &&
           hitTest(mouse,world,view) == UiAction::None;
}
UiAction Renderer::hitTest(Vector2 mouse, const World& world, const ViewState& view) const {
    if (view.help || view.paused || !world.player.alive() || world.cleared()) {
        if (inside(mouse,layout_.modalPrimary)) return view.help ? UiAction::ToggleHelp :
            (view.paused && world.player.alive() && !world.cleared() ? UiAction::TogglePause : UiAction::Restart);
        if (view.paused && !view.help && inside(mouse,layout_.modalSecondary)) return UiAction::Restart;
        return UiAction::None;
    }
    if (inside(mouse,layout_.inventoryButton)) return UiAction::ToggleInventory;
    if (inside(mouse,layout_.helpButton)) return UiAction::ToggleHelp;
    if (inside(mouse,layout_.fullscreenButton)) return UiAction::Fullscreen;
    if (inside(mouse,layout_.pauseButton)) return UiAction::TogglePause;
    if (layout_.sidebarVisible) {
        if (inside(mouse,layout_.headSlot)) return UiAction::CycleHead;
        if (inside(mouse,layout_.armorSlot)) return UiAction::CycleArmor;
        if (inside(mouse,layout_.bootsSlot)) return UiAction::CycleBoots;
        if (inside(mouse,layout_.sidebar)) return UiAction::None;
    }
    if (inside(mouse,layout_.attackButton)) return UiAction::Attack;
    if (inside(mouse,layout_.healButton)) return UiAction::Heal;
    if (inside(mouse,layout_.dashButton)) return UiAction::Dash;
    if (inside(mouse,layout_.zoomIn)) return UiAction::ZoomIn;
    if (inside(mouse,layout_.zoomOut)) return UiAction::ZoomOut;
    if (inside(mouse,layout_.resetZoom)) return UiAction::ResetZoom;
    return UiAction::None;
}
void Renderer::draw(const World& world, const ViewState& view) {
    BeginDrawing();
    ClearBackground(Background);
    BeginScissorMode(0,static_cast<int>(layout_.scene.y),static_cast<int>(layout_.scene.width),static_cast<int>(layout_.scene.height));
    // The sparse dust is procedural and fixed-seed, not a loaded texture.
    for (std::uint32_t i = 0; i < 90; ++i) {
        const float x = static_cast<float>(hash(i*13u)%10000u)/10000.0f*layout_.scene.width;
        const float y = layout_.scene.y+std::fmod(static_cast<float>(hash(i+197u)%10000u)/10000.0f*
            layout_.scene.height+time_*(1.3f+static_cast<float>(i%4u)),layout_.scene.height);
        DrawCircleV({x+std::sin(time_*0.12f+static_cast<float>(i))*8,y},i%5u==0?1.0f:0.6f,
                    alpha(i%7u==0?Gold:Muted,0.11f));
    }
    BeginMode2D(camera_);
    drawDungeon(world,view);
    EndMode2D();
    DrawRectangleGradientV(0,static_cast<int>(layout_.scene.y),static_cast<int>(layout_.scene.width),100,
                          Background,alpha(Background,0));
    DrawRectangleGradientV(0,static_cast<int>(layout_.footer.y-70),static_cast<int>(layout_.scene.width),70,
                          alpha(Background,0),Background);
    if (world.screenFlash > 0) DrawRectangleRec(layout_.scene,alpha(Red,world.screenFlash*0.22f));
    EndScissorMode();
    drawHud(world,view);
    if (view.inventory) drawSidebar(world);
    if (view.help || view.paused || !world.player.alive() || world.cleared()) drawModal(world,view);
    EndDrawing();
}
void Renderer::drawFloor(const World& world, const ViewState& view) {
    const Vector2 middle = rv(iso_.worldToScreen({10,10}));
    for (int i = 10; i > 0; --i)
        DrawEllipse(static_cast<int>(middle.x),static_cast<int>(middle.y+38),
                    515.0f+static_cast<float>(i)*8,230.0f+static_cast<float>(i)*5,Color{0,0,0,5});
    for (int y = 0; y < world.grid.height(); ++y) {
        for (int x = 0; x < world.grid.width(); ++x) {
            const TileCoord tile{x,y};
            if (!world.grid.hasFloor(tile)) continue;
            const auto corners = iso_.tileCorners(tile);
            const Vector2 a = rv(corners[0]), b = rv(corners[1]), c = rv(corners[2]), d = rv(corners[3]);
            const std::uint32_t h = world.grid.at(tile).variation;
            const int variation = static_cast<int>(h%11u);
            Color stone{static_cast<unsigned char>(37+variation),static_cast<unsigned char>(45+variation),
                        static_cast<unsigned char>(44+variation),255};
            if ((x+y)%5==0) stone = shade(stone,1.09f);
            if (x == 9 || x == 10) stone = shade(stone,1.08f);
            if (!world.grid.hasFloor({x+1,y}))
                quad(c,b,{b.x,b.y+18},{c.x,c.y+18},Color{23,29,29,255});
            if (!world.grid.hasFloor({x,y+1}))
                quad(d,c,{c.x,c.y+18},{d.x,d.y+18},Color{30,36,35,255});
            quad(a,b,c,d,stone);
            line(a,b,0.7f,Color{58,66,62,150});
            line(a,d,0.7f,Color{58,66,62,125});
            line(b,c,0.8f,Color{17,23,23,190});
            line(d,c,0.8f,Color{17,23,23,190});
            const Vector2 center = rv(iso_.worldToScreen(tile.center()));
            if (h%7u==0) {
                const Vector2 k{center.x-10,center.y-3};
                line({k.x-8,k.y-3},k,0.7f,Color{26,33,32,200});
                line(k,{k.x-3,k.y+4},0.7f,Color{26,33,32,200});
                line(k,{k.x+10,k.y+5},0.7f,Color{26,33,32,200});
            }
            if (h%5u==0) {
                diamond({center.x-11,center.y+3},5,2,Color{56,68,52,90});
                diamond({center.x+13,center.y-4},3,1.4f,Color{64,73,52,80});
            }
            if (h%17u==0) {
                for (int i = 0; i < 3; ++i)
                    diamond({center.x+static_cast<float>(i)*5-8,center.y+static_cast<float>(i%2)*2},1.8f,0.9f,
                            Color{99,103,85,65});
            }
            if (view.showGrid) diamondOutline(center,31.5f,15.5f,alpha(Teal,0.35f));
        }
    }
    // An inlaid ritual circle breaks up the stone without introducing assets.
    const Vec2 ritual{9.5f,10.0f};
    for (float radius : {2.25f,2.4f}) {
        for (int i = 0; i < 64; ++i) {
            const float a = static_cast<float>(i)*2*Pi/64;
            const float b = static_cast<float>(i+1)*2*Pi/64;
            const Vector2 p = rv(iso_.worldToScreen(ritual+Vec2{std::cos(a),std::sin(a)}*radius));
            const Vector2 q = rv(iso_.worldToScreen(ritual+Vec2{std::cos(b),std::sin(b)}*radius));
            line(p,q,0.8f,Color{144,130,86,48});
        }
    }
    for (int i = 0; i < 12; ++i) {
        const float a = static_cast<float>(i)*2*Pi/12;
        const Vector2 p = rv(iso_.worldToScreen(ritual+Vec2{std::cos(a),std::sin(a)}*2.32f));
        diamond(p,2.8f,1.5f,Color{165,144,91,70});
    }
    const Vector2 rune = rv(iso_.worldToScreen(ritual));
    diamondOutline(rune,24,12,Color{157,135,83,64});
    diamondOutline(rune,16,8,Color{157,135,83,64});
    line({rune.x-38,rune.y},{rune.x+38,rune.y},1,Color{157,135,83,50});
    line({rune.x,rune.y-19},{rune.x,rune.y+19},1,Color{157,135,83,50});
    // Warm light is painted onto the floor before any Y-sorted occluders.
    for (int y = 0; y < world.grid.height(); ++y) for (int x = 0; x < world.grid.width(); ++x) {
        if (world.grid.at({x,y}).kind != TileKind::Brazier) continue;
        const Vector2 p = rv(iso_.worldToScreen(TileCoord{x,y}.center()));
        const float pulse = 1.0f+std::sin(world.elapsed*5.0f+static_cast<float>(x))*0.06f;
        for (int i = 6; i > 0; --i)
            DrawEllipse(static_cast<int>(p.x),static_cast<int>(p.y),static_cast<float>(i)*13*pulse,
                        static_cast<float>(i)*6.5f*pulse,Color{221,137,57,static_cast<unsigned char>(i==1?13:5)});
    }
}
void Renderer::drawSector(Vec2 center, Vec2 direction, float radius, float halfArc,
                          Color fill, Color edge, float lift) const {
    const Vector2 c = add(rv(iso_.worldToScreen(center)),{0,-lift});
    Vector2 previous = add(rv(iso_.worldToScreen(center+rotated(direction,-halfArc)*radius)),{0,-lift});
    constexpr int segments = 24;
    for (int i = 1; i <= segments; ++i) {
        const float angle = -halfArc+2.0f*halfArc*static_cast<float>(i)/static_cast<float>(segments);
        const Vector2 next = add(rv(iso_.worldToScreen(center+rotated(direction,angle)*radius)),{0,-lift});
        DrawTriangle(c,next,previous,fill);
        line(previous,next,1.5f,edge);
        previous = next;
    }
    if (lift < 1) {
        line(c,add(rv(iso_.worldToScreen(center+rotated(direction,-halfArc)*radius)),{0,-lift}),1,edge);
        line(c,previous,1,edge);
    }
}
void Renderer::drawDungeon(const World& world, const ViewState& view) {
    drawFloor(world,view);
    if (sceneContains(GetMousePosition(),world,view)) {
        const TileCoord hover = tileAt(screenToWorld(GetMousePosition()));
        if (world.grid.hasFloor(hover)) {
            const Vector2 p = rv(iso_.worldToScreen(hover.center()));
            const Color color = world.grid.walkable(hover) ? Gold : Red;
            diamond(p,31,15,alpha(color,0.08f));
            diamondOutline(p,31,15,alpha(color,0.55f));
        }
    }
    const Navigation& nav = world.player.navigation;
    Vec2 previous = world.player.position;
    for (std::size_t i = nav.next; i < nav.waypoints.size(); ++i) {
        const Vec2 next = nav.waypoints[i];
        for (float f : {0.25f,0.6f,1.0f}) {
            const Vector2 p = rv(iso_.worldToScreen(previous+(next-previous)*f));
            diamond(p,1.8f,0.9f,alpha(Teal,0.6f));
        }
        previous = next;
    }
    if (nav.moving()) {
        const Vector2 p = rv(iso_.worldToScreen(nav.waypoints.back()));
        diamondOutline(p,11,5.5f,alpha(Gold,0.85f));
        diamond(p,2,1,Gold);
    }
    if (world.click.remaining > 0) {
        const Vector2 p = rv(iso_.worldToScreen(world.click.position));
        const float scale = 1+(0.8f-world.click.remaining)*1.2f;
        diamondOutline(p,12*scale,6*scale,alpha(world.click.valid?Gold:Red,world.click.remaining/0.8f));
    }
    for (const Enemy& e : world.enemies) {
        if (e.state == EnemyState::Attack && !e.attackApplied) {
            const float progress = clamp01(e.stateTime/e.windup());
            drawSector(e.position,e.attackDirection,e.attackRange()+0.25f,0.85f,
                       alpha(Red,0.07f+progress*0.17f),alpha(Red,0.3f+progress*0.6f));
        }
    }
    drawList_.clear();
    if (drawList_.capacity() < 64) drawList_.reserve(64);
    for (int y = 0; y < world.grid.height(); ++y) for (int x = 0; x < world.grid.width(); ++x) {
        const TileKind kind = world.grid.at({x,y}).kind;
        if (kind != TileKind::Floor && kind != TileKind::Void)
            drawList_.push_back({iso_.worldToScreen(TileCoord{x,y}.center()).y,0,0,{x,y}});
    }
    drawList_.push_back({iso_.worldToScreen(world.portal).y,1,0,{}});
    for (std::size_t i = 0; i < world.enemies.size(); ++i)
        drawList_.push_back({iso_.worldToScreen(world.enemies[i].position).y,2,i,{}});
    drawList_.push_back({iso_.worldToScreen(world.player.position).y,3,0,{}});
    std::stable_sort(drawList_.begin(),drawList_.end(),[](const DrawItem& a,const DrawItem& b) {
        return a.depth < b.depth;
    });
    for (const DrawItem& item : drawList_) {
        switch (item.kind) {
            case 0: drawObstacle(world,item.tile); break;
            case 1: drawPortal(world); break;
            case 2: drawEnemy(world.enemies[item.index]); break;
            case 3: drawHero(rv(iso_.worldToScreen(world.player.position)),world.player,1.0f); break;
            default: break;
        }
    }
    const Attack& attack = world.player.attack;
    if (attack.active && attack.elapsed >= 0.045f && attack.elapsed <= 0.29f) {
        const float phase = clamp01((attack.elapsed-0.045f)/0.245f);
        const float angle = -attack.halfArc()+phase*attack.halfArc()*2;
        const Vec2 direction = rotated(attack.direction,angle);
        const float fade = std::sin(phase*Pi);
        drawSector(world.player.position,direction,attack.range(),0.34f,
                   alpha(Gold,fade*0.21f),alpha(Color{255,226,157,255},fade*0.9f),19);
        drawSector(world.player.position,direction,attack.range()*0.8f,0.3f,
                   alpha(Gold,0),alpha(Ink,fade*0.6f),19);
    }
    for (const Particle& particle : world.particles) {
        const float fade = clamp01(particle.remaining/particle.lifetime);
        Vector2 p = rv(iso_.worldToScreen(particle.position));
        p.y -= std::max(0.0f,particle.height);
        const Color c = particle.kind == EffectKind::Heal ? Teal :
            (particle.kind == EffectKind::Dust ? Muted : (particle.kind == EffectKind::Hit ? Gold : Color{235,135,60,255}));
        diamond(p,particle.size*fade,particle.size*fade,alpha(c,fade));
    }
    for (const DamageNumber& number : world.damageNumbers) {
        Vector2 p = rv(iso_.worldToScreen(number.position));
        p.y -= 56+(0.85f-number.remaining)*31;
        char value[24];
        std::snprintf(value,sizeof(value),number.amount < 0 ? "+%d" : "%d",std::abs(number.amount));
        const Color color = number.amount < 0 ? Teal : (number.friendly ? Red : (number.critical ? Gold : Ink));
        const float fade = std::min(1.0f,number.remaining*4);
        const float size = number.critical ? 22.0f : 18.0f;
        centered(value,p.x+1,p.y+1,size,alpha(Background,fade));
        centered(value,p.x,p.y,size,alpha(color,fade));
    }
}
void Renderer::drawObstacle(const World& world, TileCoord tile) {
    const Tile& stone = world.grid.at(tile);
    Vector2 p = rv(iso_.worldToScreen(tile.center()));
    DrawEllipse(static_cast<int>(p.x+9),static_cast<int>(p.y+7),30,12,Color{0,0,0,55});
    if (stone.kind == TileKind::Column) {
        prism(p,24,12,7,Color{86,94,85,255},Color{51,61,57,255},Color{38,48,46,255});
        p.y -= 7;
        prism(p,17,8,58,Color{104,111,99,255},Color{66,78,71,255},Color{43,56,53,255});
        line({p.x-8,p.y-52},{p.x-8,p.y-9},1,Color{95,107,93,140});
        line({p.x+8,p.y-52},{p.x+8,p.y-9},1,Color{26,39,36,150});
        line({p.x-5,p.y-38},{p.x+1,p.y-31},1,Color{29,40,37,230});
        line({p.x+1,p.y-31},{p.x-2,p.y-23},1,Color{29,40,37,230});
        p.y -= 58;
        prism(p,21,11,9,Color{110,117,103,255},Color{74,86,75,255},Color{48,62,56,255});
        diamond({p.x-9,p.y-7},6,3,Color{99,119,75,180});
    } else if (stone.kind == TileKind::Rubble) {
        prism(p,24,11,9,Color{84,90,79,255},Color{52,61,53,255},Color{41,51,46,255});
        prism({p.x-9,p.y-9},12,7,10,Color{97,103,90,255},Color{63,73,61,255},Color{45,56,48,255});
        prism({p.x+12,p.y-4},9,6,6,Color{76,85,73,255},Color{47,61,49,255},Color{35,48,41,255});
        diamond({p.x-17,p.y-12},5,2,Color{108,123,75,160});
        diamond({p.x+22,p.y+9},3,1.5f,Color{75,85,66,255});
        line({p.x-15,p.y-9},{p.x-7,p.y-5},1,Color{46,57,47,255});
    } else if (stone.kind == TileKind::Brazier) {
        prism(p,13,7,16,Color{88,82,64,255},Color{58,59,47,255},Color{44,48,41,255});
        p.y -= 16;
        prism(p,19,9,7,Color{139,105,57,255},Color{85,74,44,255},Color{63,57,38,255});
        p.y -= 10;
        DrawCircleGradient(static_cast<int>(p.x),static_cast<int>(p.y-10),36,Color{255,155,65,24},Color{255,155,65,0});
        const float dance = std::sin(world.elapsed*9+static_cast<float>(tile.x))*3;
        polygon({{p.x-10,p.y+3},{p.x-12,p.y-5},{p.x-3,p.y-18},{p.x+1,p.y-9},
                 {p.x+3+dance,p.y-31},{p.x+11,p.y-10},{p.x+8,p.y+2}},Color{221,126,46,255});
        polygon({{p.x-6,p.y+2},{p.x-3,p.y-12},{p.x+1+dance,p.y-22},
                 {p.x+5,p.y-7},{p.x+6,p.y+2}},Color{255,192,78,255});
        polygon({{p.x-3,p.y+3},{p.x+1,p.y-10},{p.x+4,p.y+3}},Color{255,228,153,255});
        for (int i = 0; i < 4; ++i) {
            const float phase = std::fmod(world.elapsed*0.65f+static_cast<float>(i)*0.29f,1.0f);
            const float x = p.x+std::sin(phase*8+static_cast<float>(i))*9;
            diamond({x,p.y-20-phase*35},1.1f,1.7f,alpha(Gold,1-phase));
        }
    }
}
void Renderer::drawPortal(const World& world) {
    const Vector2 p = rv(iso_.worldToScreen(world.portal));
    const Color glow = world.cleared() ? Gold : Color{110,155,145,255};
    DrawEllipse(static_cast<int>(p.x),static_cast<int>(p.y),51,22,alpha(glow,0.06f));
    prism({p.x,p.y+4},40,19,8,Color{65,76,70,255},Color{42,53,49,255},Color{32,44,41,255});
    for (float x : {-30.0f,30.0f}) {
        prism({p.x+x,p.y-3},10,5,61,Color{92,104,91,255},Color{66,79,68,255},Color{41,56,48,255});
        prism({p.x+x,p.y-62},13,7,7,Color{109,119,100,255},Color{78,91,75,255},Color{51,67,56,255});
    }
    for (int i = 5; i >= 0; --i)
        DrawEllipseLines(static_cast<int>(p.x),static_cast<int>(p.y-34),25+static_cast<float>(i)*1.1f,
                         38+static_cast<float>(i)*0.65f,alpha(glow,i==0?0.7f:0.045f));
    for (int i = 0; i < 10; ++i) {
        const float a = static_cast<float>(i)*2*Pi/10+world.elapsed*0.08f;
        diamond({p.x+std::cos(a)*26,p.y-34+std::sin(a)*38},1.5f,2.5f,alpha(glow,0.65f));
    }
    diamond({p.x,p.y-34},6,13,alpha(glow,0.35f+std::sin(world.elapsed*1.7f)*0.1f));
}
void Renderer::drawHero(Vector2 feet, const Player& player, float scale, bool preview) const {
    const float gait = !preview && player.walking ? std::sin(player.animation)*2.5f : 0;
    const float breath = std::sin((preview?time_:player.animation)*2.0f)*0.6f;
    auto p = [feet,scale](float x,float y) { return Vector2{feet.x+x*scale,feet.y+y*scale}; };
    auto shape = [&p](std::initializer_list<Vec2> points,Color color) {
        std::array<Vector2,16> result{};
        std::size_t i = 0;
        for (Vec2 point : points) result[i++] = p(point.x,point.y);
        polygon(result.data(),i,color);
    };
    const bool flash = !preview && player.flash > 0 && static_cast<int>(player.flash*50)%2==0;
    Color armor = flash ? Ink : tint(player.equipment.get(EquipmentSlot::Armor).tint);
    Color head = flash ? Ink : tint(player.equipment.get(EquipmentSlot::Head).tint);
    Color boots = flash ? Ink : tint(player.equipment.get(EquipmentSlot::Boots).tint);
    const int armorVariant = player.equipment.variant(EquipmentSlot::Armor);
    const int headVariant = player.equipment.variant(EquipmentSlot::Head);
    const int bootsVariant = player.equipment.variant(EquipmentSlot::Boots);
    DrawEllipse(static_cast<int>(feet.x),static_cast<int>(feet.y+2*scale),18*scale,7*scale,Color{0,0,0,95});
    if (!preview && player.alive()) {
        DrawEllipseLines(static_cast<int>(feet.x),static_cast<int>(feet.y),20*scale,9*scale,alpha(Teal,0.75f));
        diamond(p(0,5),2*scale,1*scale,Teal);
    }
    if (!player.alive() && !preview) {
        shape({{-21,-2},{-3,-11},{20,1},{-5,6}},shade(armor,0.55f));
        DrawCircleV(p(15,-4),6*scale,head);
        return;
    }
    if (!preview && player.dashRemaining > 0) {
        const Vec2 d = normalized(iso_.worldToScreen(player.facing)-iso_.origin);
        for (int i = 3; i > 0; --i) {
            const Vector2 ghost{feet.x-d.x*static_cast<float>(i)*14,feet.y-d.y*static_cast<float>(i)*14};
            diamond({ghost.x,ghost.y-21*scale},12*scale,22*scale,alpha(Teal,0.06f*static_cast<float>(4-i)));
        }
    }
    // Base body / cloak. All visible equipment is layered in slot order below.
    shape({{-10,-34},{10,-34},{14,-4},{3,-9},{-9,-2},{-14,-6}},shade(armor,armorVariant==2?0.65f:0.42f));
    line(p(-5,-22),p(-6,-6),5*scale,Color{80,72,59,255});
    line(p(5,-22),p(6,-6),5*scale,Color{80,72,59,255});
    // Boots.
    const float bootHeight = bootsVariant == 1 ? 14.0f : 10.0f;
    quad(p(-9,-bootHeight+gait),p(-2,-bootHeight+gait),p(-2,1+gait),p(-11,1+gait),boots);
    quad(p(2,-bootHeight-gait),p(9,-bootHeight-gait),p(11,1-gait),p(2,1-gait),shade(boots,0.76f));
    line(p(-10,1+gait),p(-2,1+gait),2*scale,shade(boots,0.48f));
    line(p(2,1-gait),p(11,1-gait),2*scale,shade(boots,0.43f));
    if (bootsVariant == 2) {
        line(p(-9,-6+gait),p(-3,-6+gait),1.5f*scale,Gold);
        line(p(3,-6-gait),p(9,-6-gait),1.5f*scale,Gold);
    }
    // Armor, shoulder pieces, and bracers.
    shape({{-8,-36+breath},{8,-36+breath},{12,-29},{8,-15},{-8,-15},{-12,-29}},shade(armor,0.65f));
    shape({{-7,-34+breath},{7,-34+breath},{8,-24},{4,-16},{-7,-18},{-9,-26}},armor);
    shape({{0,-33+breath},{7,-32+breath},{8,-24},{4,-16},{0,-17}},shade(armor,0.78f));
    line(p(-12,-28),p(-15,-18),5*scale,shade(armor,0.8f));
    line(p(12,-28),p(15,-18),5*scale,shade(armor,0.7f));
    DrawCircleV(p(-12,-31+breath),5.5f*scale,shade(armor,1.12f));
    DrawCircleV(p(12,-31+breath),5.5f*scale,shade(armor,0.8f));
    line(p(-8,-16),p(8,-16),3*scale,Color{75,62,43,255});
    DrawRectangleRec({p(-2,-18).x,p(-2,-18).y,4*scale,4*scale},Gold);
    if (armorVariant == 1) {
        line(p(-6,-29),p(6,-29),1*scale,shade(armor,1.4f));
        line(p(-6,-24),p(6,-24),1*scale,shade(armor,1.3f));
        diamond(p(0,-31),2*scale,3*scale,Gold);
    } else if (armorVariant == 2) {
        line(p(-8,-34),p(7,-18),2*scale,Gold);
        shape({{9,-33},{17,-28},{13,-12},{7,-17}},shade(armor,0.9f));
    } else {
        line(p(-7,-33),p(6,-17),2*scale,Color{186,149,96,255});
    }
    // Head. The variants intentionally alter silhouette, not just tint.
    if (headVariant == 0) {
        shape({{0,-55+breath},{9,-47+breath},{9,-35+breath},{-9,-35+breath},{-10,-45+breath}},shade(head,0.56f));
        shape({{0,-53+breath},{7,-46+breath},{7,-37+breath},{-7,-37+breath},{-8,-45+breath}},head);
        shape({{0,-49+breath},{5,-44+breath},{4,-38+breath},{-4,-38+breath},{-5,-44+breath}},Color{43,43,35,255});
        line(p(-3,-43+breath),p(3,-43+breath),1.2f*scale,Color{224,186,125,255});
        line(p(-7,-36+breath),p(6,-36+breath),2*scale,shade(head,0.65f));
    } else if (headVariant == 1) {
        shape({{-8,-47+breath},{-3,-53+breath},{5,-52+breath},{10,-45+breath},{8,-35+breath},{-7,-35+breath}},head);
        shape({{2,-52+breath},{10,-45+breath},{8,-35+breath},{2,-36+breath}},shade(head,0.63f));
        line(p(-6,-43+breath),p(7,-43+breath),2*scale,Color{30,37,36,255});
        line(p(1,-44+breath),p(1,-36+breath),2*scale,shade(head,1.25f));
        line(p(-4,-52+breath),p(2,-57+breath),3*scale,Color{167,73,59,255});
    } else {
        DrawCircleV(p(0,-42+breath),7*scale,Color{187,151,103,255});
        shape({{-8,-46+breath},{-9,-54+breath},{-4,-49+breath},{0,-57+breath},
               {4,-49+breath},{9,-54+breath},{8,-46+breath}},head);
        line(p(-7,-45+breath),p(7,-45+breath),2*scale,Gold);
        diamond(p(0,-49+breath),1.8f*scale,2.8f*scale,Red);
        line(p(-3,-42+breath),p(3,-42+breath),1.5f*scale,Color{48,40,29,255});
    }
    // Weapon follows the projected facing; the arc itself is drawn in world space.
    Vec2 direction = preview ? Vec2{0.85f,-0.45f} : normalized(iso_.worldToScreen(player.facing)-iso_.origin);
    if (!preview && player.attack.active) {
        const float phase = clamp01(player.attack.elapsed/0.29f);
        const Vec2 facing = rotated(player.attack.direction,-player.attack.halfArc()+phase*2*player.attack.halfArc());
        direction = normalized(iso_.worldToScreen(facing)-iso_.origin);
    }
    const Vector2 hand = p(direction.x >= 0 ? 15.0f : -15.0f,-20);
    const Vector2 tip{hand.x+direction.x*27*scale,hand.y+direction.y*27*scale};
    line({hand.x-direction.x*5*scale,hand.y-direction.y*5*scale},hand,3*scale,Color{121,88,52,255});
    line(hand,tip,4*scale,Color{93,111,111,255});
    line({hand.x-0.7f*scale,hand.y-0.7f*scale},{tip.x-0.7f*scale,tip.y-0.7f*scale},1.1f*scale,Color{219,224,204,255});
    line({hand.x-direction.y*5*scale,hand.y+direction.x*5*scale},
         {hand.x+direction.y*5*scale,hand.y-direction.x*5*scale},2.5f*scale,Gold);
}
void Renderer::drawEnemy(const Enemy& enemy) const {
    const Vector2 feet = rv(iso_.worldToScreen(enemy.position));
    const float s = enemy.kind == EnemyKind::Warden ? 1.33f : 1.0f;
    const float gait = enemy.state == EnemyState::Chase ? std::sin(enemy.animation)*2 : 0;
    auto p = [feet,s](float x,float y) { return Vector2{feet.x+x*s,feet.y+y*s}; };
    auto shape = [&p](std::initializer_list<Vec2> points,Color color) {
        std::array<Vector2,16> result{};
        std::size_t i = 0;
        for (Vec2 point : points) result[i++] = p(point.x,point.y);
        polygon(result.data(),i,color);
    };
    const float fade = enemy.alive() ? 1 : clamp01(1-enemy.stateTime/0.65f);
    const bool flashing = enemy.flash > 0;
    const Color cloth = flashing ? Ink : (enemy.kind == EnemyKind::Warden ? Color{89,72,69,255} : Color{122,66,57,255});
    DrawEllipse(static_cast<int>(feet.x),static_cast<int>(feet.y+2),16*s,6*s,Color{0,0,0,90});
    if (!enemy.alive()) {
        shape({{-19,-3},{-5,-10},{14,-4},{18,3},{-7,6}},alpha(cloth,fade));
        DrawCircleV(p(14,-4),5*s,alpha(Color{170,156,125,255},fade));
        return;
    }
    const Vec2 cursor = screenToWorld(GetMousePosition());
    const bool hover = distance(cursor,enemy.position) < 0.6f;
    if (hover) DrawEllipseLines(static_cast<int>(feet.x),static_cast<int>(feet.y),18*s,8*s,alpha(Red,0.8f));
    shape({{-9,-30},{8,-30},{13,-4},{3,-7},{-3,-2},{-12,-5}},shade(cloth,0.66f));
    line(p(-5,-19),p(-7,-1+gait),5*s,Color{71,64,56,255});
    line(p(5,-19),p(7,-1-gait),5*s,Color{65,58,51,255});
    shape({{-7,-33},{6,-33},{11,-26},{8,-15},{-8,-15},{-11,-26}},cloth);
    shape({{1,-31},{7,-31},{10,-26},{7,-16},{1,-17}},shade(cloth,0.68f));
    line(p(-9,-28),p(-15,-17),4*s,shade(cloth,0.84f));
    line(p(10,-28),p(15,-16),4*s,shade(cloth,0.73f));
    line(p(-7,-17),p(7,-17),2*s,Color{148,118,77,255});
    shape({{-6,-44},{1,-48},{7,-42},{6,-34},{-5,-34}},flashing?Ink:Color{174,159,127,255});
    shape({{1,-47},{7,-42},{6,-34},{1,-35}},flashing?Ink:Color{115,109,91,255});
    line(p(-4,-41),p(-1,-40),1.4f*s,Color{230,110,61,255});
    line(p(2,-40),p(5,-41),1.4f*s,Color{230,110,61,255});
    if (enemy.kind == EnemyKind::Warden) {
        shape({{-7,-43},{-13,-56},{-7,-52},{-2,-44}},Color{149,139,111,255});
        shape({{6,-43},{13,-56},{7,-52},{2,-44}},Color{123,121,104,255});
        diamond(p(-11,-29),5*s,7*s,Color{126,115,92,255});
        diamond(p(11,-29),5*s,7*s,Color{103,101,86,255});
    } else {
        line(p(-5,-44),p(-9,-50),2*s,Color{133,127,104,255});
        line(p(6,-43),p(10,-49),2*s,Color{133,127,104,255});
    }
    Vec2 direction = normalized(iso_.worldToScreen(enemy.facing)-iso_.origin);
    if (enemy.state == EnemyState::Attack && !enemy.attackApplied)
        direction = normalized(Vec2{direction.x,-1.7f});
    const Vector2 hand = p(direction.x>=0?14.0f:-14.0f,-18);
    const Vector2 end{hand.x+direction.x*23*s,hand.y+direction.y*23*s};
    line(hand,end,3*s,Color{139,133,112,255});
    if (enemy.kind == EnemyKind::Warden) {
        line({end.x-6*s,end.y-4*s},{end.x+6*s,end.y+4*s},7*s,Color{137,124,96,255});
    }
    if (hover || enemy.health < enemy.maxHealth || enemy.state != EnemyState::Idle) {
        const float width = enemy.kind == EnemyKind::Warden?44.0f:34.0f;
        const float y = feet.y-57*s;
        if (enemy.kind == EnemyKind::Warden) centered("ASH WARDEN",feet.x,y-13,9,Gold,1.3f);
        DrawRectangleRec({feet.x-width*0.5f-1,y-1,width+2,5},Background);
        DrawRectangleRec({feet.x-width*0.5f,y,width*static_cast<float>(enemy.health)/static_cast<float>(enemy.maxHealth),3},Red);
    }
}
void Renderer::drawHud(const World& world, const ViewState& view) const {
    const float w = layout_.header.width;
    const bool compact = layout_.scene.width < 760;
    const float sceneWidth = layout_.scene.width;
    DrawRectangleRec(layout_.header,Color{13,18,20,255});
    horizontal(0,layout_.header.height-1,w);
    const float logoX = w<760?25.0f:38.0f;
    icon(8,{logoX,layout_.header.height*0.5f-2},w<760?24.0f:30.0f,Gold);
    text("ASHFALL",logoX+27,w<760?22.0f:21.0f,w<760?20.0f:26.0f,Ink,w<760?3.0f:4.0f);
    if (w>=760) text("CHRONICLES OF THE HOLLOW",logoX+28,52,8,Muted,1.5f);
    const auto tab = [](Rectangle r,const char* label,bool active) {
        const bool hovered = inside(GetMousePosition(),r);
        if (hovered) DrawRectangleRec(r,alpha(Raised,0.55f));
        centered(label,r.x+r.width*0.5f,r.y+r.height*0.5f-5,10,active?Gold:Muted,1.4f);
        if (active) horizontal(r.x+19,r.y+r.height-1,r.width-38,Gold);
    };
    tab(layout_.inventoryButton,w<760?"GEAR":"CHARACTER",view.inventory);
    tab(layout_.helpButton,w<760?"HELP":"FIELD GUIDE",view.help);
    if (w>=1000) {
        DrawCircleV({w-203,layout_.header.height*0.5f},2.5f,Teal);
#ifdef __EMSCRIPTEN__
        text("WEBASSEMBLY",w-191,layout_.header.height*0.5f-4,9,Muted,1.5f);
#else
        text("NATIVE BUILD",w-191,layout_.header.height*0.5f-4,9,Muted,1.5f);
#endif
        line({w-67,23},{w-67,layout_.header.height-23},1,Border);
    }
    icon(9,{layout_.fullscreenButton.x+18,layout_.fullscreenButton.y+18},17,
         inside(GetMousePosition(),layout_.fullscreenButton)?Gold:Muted);
    const float titleX = compact?22.0f:34.0f;
    const float titleY = layout_.scene.y+27;
    text("ACT I  /  THE HOLLOW LANDS",titleX,titleY,9,Gold,1.6f);
    text("The Ashen Sanctum",titleX,titleY+23,compact?20.0f:26.0f,Ink,1.0f);
    if (!compact) text("A forgotten place. An unfinished oath.",titleX,titleY+61,11,Muted,0.8f);
    if (sceneWidth>=800) {
        const float qx = sceneWidth-249;
        icon(7,{qx+10,titleY+11},19,Gold);
        text("SANCTUM OBJECTIVE",qx+31,titleY+2,9,Muted,1.4f);
        text(world.cleared()?"The hollow are silent":"Silence the hollow",qx+31,titleY+21,14,Ink);
        DrawRectangleRec({qx+31,titleY+48,150,3},Border);
        DrawRectangleRec({qx+31,titleY+48,150*static_cast<float>(world.kills)/
                          static_cast<float>(std::max(1,world.totalEnemies)),3},Gold);
        char progress[32];
        std::snprintf(progress,sizeof(progress),"%d / %d",world.kills,world.totalEnemies);
        text(progress,qx+188,titleY+44,11,Gold);
    } else {
        char progress[32];
        std::snprintf(progress,sizeof(progress),"HOLLOW  %d / %d",world.kills,world.totalEnemies);
        text(progress,titleX,titleY+55,9,Muted,1.2f);
    }
    if (world.noticeRemaining > 0 && !view.paused && world.player.alive()) {
        const float width = textWidth(world.notice.c_str(),11,0.5f)+34;
        const float x = (sceneWidth-width)*0.5f;
        const float y = layout_.footer.y-94;
        DrawRectangleRec({x,y,width,30},alpha(Background,0.88f));
        line({x,y},{x+width,y},1,alpha(Gold,0.3f));
        centered(world.notice.c_str(),sceneWidth*0.5f,y+10,11,Gold,0.5f);
    }
    if (!compact) {
        text("LMB",34,layout_.footer.y-31,10,Gold,1.3f);
        text("MOVE",68,layout_.footer.y-31,9,Muted,1.1f);
        text("RMB / SPACE",132,layout_.footer.y-31,10,Gold,1.1f);
        text("ATTACK",228,layout_.footer.y-31,9,Muted,1.1f);
        const char* controls = "[G] GRID  /  SCROLL TO ZOOM";
        if (sceneWidth>1000) text(controls,sceneWidth*0.5f-74,layout_.footer.y-31,8,Dim,1.1f);
    }
    for (const auto& b : {std::make_pair(layout_.zoomOut,"-"),std::make_pair(layout_.resetZoom,"o"),
                          std::make_pair(layout_.zoomIn,"+")}) {
        DrawRectangleRec(b.first,inside(GetMousePosition(),b.first)?Raised:alpha(Panel,0.8f));
        DrawRectangleLinesEx(b.first,1,Border);
        centered(b.second,b.first.x+15,b.first.y+8,14,Muted);
    }
    // Persistent action bar. Buttons and keyboard commands share the same actions.
    DrawRectangleRec(layout_.footer,Panel);
    horizontal(0,layout_.footer.y,sceneWidth);
    const float fy = layout_.footer.y;
    const float orbX = compact?32.0f:52.0f;
    const float orbY = fy+(compact?43.0f:49.0f);
    const float radius = compact?22.0f:27.0f;
    DrawCircleV({orbX,orbY},radius+3,Background);
    DrawCircleV({orbX,orbY},radius,Color{76,38,35,255});
    DrawCircleGradient(static_cast<int>(orbX-5),static_cast<int>(orbY-5),radius,
                       Color{154,65,54,255},Color{88,37,35,255});
    DrawCircleLines(static_cast<int>(orbX),static_cast<int>(orbY),radius+2,shade(Gold,0.65f));
    icon(8,{orbX,orbY},radius*0.9f,alpha(Gold,0.75f));
    const float healthX = compact?65.0f:96.0f;
    const float healthWidth = compact?std::max(62.0f,layout_.attackButton.x-healthX-19):188;
    text(compact?"VITALITY":"THE WAYFARER",healthX,fy+24,compact?8.0f:10.0f,Muted,1.2f);
    char health[40];
    std::snprintf(health,sizeof(health),"%d / %d",world.player.health,Player::MaxHealth);
    text(health,healthX,fy+42,compact?11.0f:15.0f,Ink,0.7f);
    DrawRectangleRec({healthX,fy+64,healthWidth,5},Background);
    DrawRectangleRec({healthX,fy+64,healthWidth*static_cast<float>(world.player.health)/Player::MaxHealth,5},Red);
    DrawRectangleRec({healthX,fy+73,healthWidth,2},Border);
    DrawRectangleRec({healthX,fy+73,healthWidth*static_cast<float>(world.player.experience%100)/100.0f,2},Gold);
    if (!compact) {
        char level[30];
        std::snprintf(level,sizeof(level),"LVL %02d",world.player.level());
        text(level,healthX+healthWidth-47,fy+26,9,Gold);
    }
    const auto ability = [](Rectangle r,int symbol,const char* key,Color color,float cooldown,const char* label) {
        const bool hovered = inside(GetMousePosition(),r);
        DrawRectangleRec(r,hovered?shade(Raised,1.2f):Raised);
        DrawRectangleLinesEx(r,1,hovered?shade(Gold,0.8f):Border);
        icon(symbol,{r.x+r.width*0.5f,r.y+r.height*0.48f},r.width*0.47f,color);
        if (cooldown>0) {
            DrawRectangleRec(r,alpha(Background,0.65f));
            char value[16];
            std::snprintf(value,sizeof(value),"%.1f",static_cast<double>(cooldown));
            centered(value,r.x+r.width*0.5f,r.y+r.height*0.45f-5,12,Ink);
        }
        const float keyWidth = textWidth(key,8)+9;
        DrawRectangleRec({r.x+(r.width-keyWidth)*0.5f,r.y+r.height-7,keyWidth,14},Background);
        centered(key,r.x+r.width*0.5f,r.y+r.height-4,8,Muted);
        if (hovered) centered(label,r.x+r.width*0.5f,r.y-17,9,Ink);
    };
    ability(layout_.attackButton,0,"SPACE",Gold,world.player.attack.cooldown,"CLEAVE");
    ability(layout_.healButton,4,"Q",Red,0,"HEAL +50");
    ability(layout_.dashButton,5,"SHIFT",Teal,world.player.dashCooldown,"EVASIVE DASH");
    char count[12];
    std::snprintf(count,sizeof(count),"%d",world.player.potions);
    text(count,layout_.healButton.x+layout_.healButton.width-12,layout_.healButton.y+5,9,world.player.potions?Gold:Dim);
    icon(6,{layout_.pauseButton.x+layout_.pauseButton.width*0.5f,
            layout_.pauseButton.y+layout_.pauseButton.height*0.5f},19,
         inside(GetMousePosition(),layout_.pauseButton)?Gold:Muted);
    if (!compact) {
        text("ESC",layout_.pauseButton.x+7,layout_.pauseButton.y+39,8,Dim);
        text("DEPTH 01",34,fy+91,8,Dim,1.4f);
        const char* oath = "KEEP THE FLAME ALIVE";
        text(oath,sceneWidth-textWidth(oath,8,1.4f)-28,fy+91,8,Dim,1.4f);
    } else text("CLICK TO MOVE  /  RMB TO ATTACK",22,fy+84,8,Dim,0.7f);
}
void Renderer::drawSidebar(const World& world) const {
    const Rectangle side = layout_.sidebar;
    BeginScissorMode(static_cast<int>(side.x),static_cast<int>(side.y),static_cast<int>(side.width),static_cast<int>(side.height));
    DrawRectangleRec(side,Color{17,22,24,255});
    line({side.x,side.y},{side.x,side.y+side.height},1,Border);
    const float x = side.x+22, y = side.y+27;
    text("YOUR CHARACTER",x,y,9,Muted,1.6f);
    text("The Wayfarer",x,y+24,21,Ink,0.5f);
    icon(1,{side.x+side.width-31,y+29},18,shade(Gold,0.8f));
    const float centerX = side.x+side.width*0.5f;
    const float avatarY = side.y+218;
    DrawCircleGradient(static_cast<int>(centerX),static_cast<int>(side.y+158),83,Color{68,75,57,24},Color{68,75,57,0});
    DrawEllipseLines(static_cast<int>(centerX),static_cast<int>(avatarY+3),64,23,Color{47,57,53,255});
    DrawEllipseLines(static_cast<int>(centerX),static_cast<int>(avatarY+3),56,19,Color{36,46,43,255});
    for (float dx : {-80.0f,80.0f}) {
        line({centerX+dx,side.y+112},{centerX+dx,side.y+204},1,Color{35,45,43,255});
        diamond({centerX+dx,side.y+106},2,3,shade(Gold,0.5f));
    }
    drawHero({centerX,avatarY},world.player,1.65f,true);
    char level[30];
    std::snprintf(level,sizeof(level),"LEVEL %02d",world.player.level());
    centered(level,centerX,side.y+87,8,Gold,1.7f);
    const auto slot = [&](Rectangle r,EquipmentSlot equipmentSlot,int symbol,const char* title,const char* key) {
        const EquipmentPiece& gear = world.player.equipment.get(equipmentSlot);
        const bool hover = inside(GetMousePosition(),r);
        DrawRectangleRec(r,hover?Color{31,38,38,255}:Color{22,28,30,255});
        DrawRectangleLinesEx(r,1,hover?shade(Gold,0.65f):Border);
        DrawRectangleRec({r.x+9,r.y+11,43,44},Color{15,20,22,255});
        diamondOutline({r.x+30.5f,r.y+33},19,19,alpha(tint(gear.tint),0.16f));
        icon(symbol,{r.x+30.5f,r.y+33},24,tint(gear.tint));
        text(title,r.x+64,r.y+10,8,Muted,1.5f);
        text(gear.name.data(),r.x+64,r.y+25,13,Ink,0.3f);
        char armor[40];
        std::snprintf(armor,sizeof(armor),"+%d ARMOR",gear.armor);
        text(armor,r.x+64,r.y+45,8,Gold,0.9f);
        text(key,r.x+r.width-20,r.y+10,8,Dim);
        text(">",r.x+r.width-18,r.y+36,11,hover?Gold:Dim);
    };
    slot(layout_.headSlot,EquipmentSlot::Head,3,"HEAD","1");
    slot(layout_.armorSlot,EquipmentSlot::Armor,1,"ARMOR","2");
    slot(layout_.bootsSlot,EquipmentSlot::Boots,2,"BOOTS","3");
    centered("CLICK A SLOT TO CHANGE EQUIPMENT",centerX,side.y+488,8,Dim,0.7f);
    horizontal(x,side.y+516,side.width-44);
    const float statsY = side.y+538;
    const float gap = (side.width-44)/3;
    const std::array<const char*,3> labels = {{"DAMAGE","ARMOR","SPEED"}};
    for (std::size_t i = 0; i < labels.size(); ++i) {
        const float cx = x+gap*(static_cast<float>(i)+0.5f);
        centered(labels[i],cx,statsY,8,Muted,1.0f);
        char value[24];
        if (i==0) std::snprintf(value,sizeof(value),"%d",world.player.damage());
        else if (i==1) std::snprintf(value,sizeof(value),"%d",world.player.equipment.armor());
        else std::snprintf(value,sizeof(value),"%.1f",static_cast<double>(world.player.moveSpeed()));
        centered(value,cx,statsY+20,18,i==1?Gold:Ink);
        if (i<2) line({x+gap*static_cast<float>(i+1),statsY+4},
                     {x+gap*static_cast<float>(i+1),statsY+36},1,Border);
    }
    const float bottom = side.y+side.height;
    if (side.height > 740) {
        horizontal(x,bottom-206,side.width-44);
        text("SANCTUM MAP",x,bottom-186,8,Muted,1.3f);
        text("N",side.x+side.width-32,bottom-186,8,Gold);
        drawMinimap(world,{x,bottom-163,side.width-44,111});
    }
    char embers[40];
    std::snprintf(embers,sizeof(embers),"%d EMBERS",world.player.embers);
    icon(8,{x+7,bottom-26},14,Gold);
    text(embers,x+22,bottom-30,9,Gold,1.1f);
    text("[I] HIDE",side.x+side.width-76,bottom-30,8,Dim);
    EndScissorMode();
}
void Renderer::drawMinimap(const World& world,Rectangle bounds) const {
    const float scale = std::min(bounds.width/1170.0f,bounds.height/570.0f);
    const Vector2 origin{bounds.x+bounds.width*0.5f,bounds.y+bounds.height*0.5f-320*scale};
    const auto point = [this,origin,scale](Vec2 p) {
        const Vec2 iso = iso_.worldToScreen(p);
        return Vector2{origin.x+iso.x*scale,origin.y+iso.y*scale};
    };
    for (int y = 0; y < world.grid.height(); ++y) for (int x = 0; x < world.grid.width(); ++x) {
        if (!world.grid.hasFloor({x,y})) continue;
        const Vector2 p = point(TileCoord{x,y}.center());
        diamond(p,29*scale,14*scale,world.grid.walkable({x,y})?Color{47,60,56,255}:Color{74,83,70,255});
    }
    for (const Enemy& e : world.enemies)
        if (e.alive()) DrawCircleV(point(e.position),e.kind==EnemyKind::Warden?2.0f:1.4f,Red);
    const Vector2 player = point(world.player.position);
    DrawCircleV(player,3.4f,alpha(Teal,0.2f));
    DrawCircleV(player,1.8f,Teal);
    diamond(point(world.portal),2.2f,2.2f,Gold);
}
void Renderer::drawModal(const World& world,const ViewState& view) const {
    DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{5,9,11,207});
    const Rectangle r = layout_.modal;
    DrawRectangleRec(r,Panel);
    DrawRectangleLinesEx(r,1,Border);
    horizontal(r.x+1,r.y,r.width-2,Gold);
    const float cx = r.x+r.width*0.5f;
    icon(8,{cx,r.y+43},24,Gold);
    if (view.help) {
        centered("A FLAME WORTH KEEPING",cx,r.y+77,18,Ink,1.5f);
        centered("Enter the sanctum. Silence the hollow. Survive.",cx,r.y+108,10,Muted,0.7f);
        struct Row { const char* key; const char* title; const char* detail; };
        const std::array<Row,6> rows = {{{"LMB","Move","A* finds a path around the ruins."},
            {"RMB / SPACE","Cleave","Aim with the mouse. Hold to chain 3 strikes."},
            {"SHIFT","Dodge","Dash toward the cursor. Briefly invulnerable."},
            {"Q","Heal","Recover 50 vitality. Three flasks per run."},
            {"1 / 2 / 3","Equip","Cycle head, armor, and boots. Stats are live."},
            {"ESC / P","Pause","I: character   G: grid   R: restart"}}};
        const float keyWidth = r.width>450?100.0f:82.0f;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const float y = r.y+146+static_cast<float>(i)*37;
            keycap(rows[i].key,r.x+30,y,keyWidth);
            text(rows[i].title,r.x+keyWidth+46,y,11,Ink);
            if (r.width>430) text(rows[i].detail,r.x+keyWidth+46,y+15,8,Muted,0.1f);
        }
        button(layout_.modalPrimary,"ENTER THE SANCTUM",true);
    } else if (!world.player.alive()) {
        centered("YOUR FLAME FADES",cx,r.y+80,22,Ink,2);
        centered("Even a fading ember can be rekindled.",cx,r.y+117,11,Muted,0.5f);
        char progress[80];
        std::snprintf(progress,sizeof(progress),"%d HOLLOW SILENCED  /  %d EMBERS",world.kills,world.player.embers);
        centered(progress,cx,r.y+158,10,Gold,1.0f);
        button(layout_.modalPrimary,"REKINDLE  /  NEW RUN",true);
    } else if (world.cleared()) {
        centered("SANCTUM CLEARED",cx,r.y+80,23,Ink,2);
        centered("The hollow are silent. Your oath endures.",cx,r.y+118,11,Muted,0.5f);
        char progress[90];
        const int seconds = static_cast<int>(world.elapsed);
        std::snprintf(progress,sizeof(progress),"%d EMBERS  /  %02d:%02d  /  LEVEL %d",
                      world.player.embers,seconds/60,seconds%60,world.player.level());
        centered(progress,cx,r.y+159,11,Gold);
        button(layout_.modalPrimary,"DESCEND AGAIN",true);
    } else {
        centered("A MOMENT OF RESPITE",cx,r.y+79,21,Ink,1.4f);
        centered("The ruins can wait. Keep your flame.",cx,r.y+118,11,Muted,0.5f);
        button(layout_.modalSecondary,"ABANDON RUN  /  RESTART");
        button(layout_.modalPrimary,"RETURN TO THE SANCTUM",true);
    }
}

} // namespace ashfall

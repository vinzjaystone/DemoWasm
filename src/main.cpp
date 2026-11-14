#include "renderer.h"
#include "equipment_browser.h"
#include "run_save.h"
#include "snapshot_browser.h"
#include "local_store.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdio>
#include <memory>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace ashfall {
class Application {
public:
    World world;
    ViewState view;
    Renderer renderer;
    Vec2 aim{10.5f,12.5f};
    std::uint32_t artSerial=0;
    std::string artName="built-in";
    std::uint32_t artHash=0;
    int saveStatus=0;
    std::uint32_t saveSerial=0,loadSerial=0,saveEventSerial=0;
    std::uint32_t restartSerial = 0; // Read-only acknowledgement of processed restart inputs.
    std::uint32_t groundSerial = 0; // Acknowledge ground clicks separately from simulation time.

    Application() {
        view.inventory = GetScreenWidth() >= 960;
        renderer.initializePlayer(world.player);
        renderer.prepare(world,view,0);
        world.notify("Your oath begins here.",3.0f);
    }
    void action(UiAction command) {
        switch (command) {
            case UiAction::ToggleInventory: view.inventory = !view.inventory; break;
            case UiAction::ToggleHelp: view.help = !view.help; break;
            case UiAction::TogglePause: view.paused = !view.paused; break;
            case UiAction::SaveRun: saveRun(false); break;
            case UiAction::LoadRun: loadRun(); break;
            case UiAction::ExportRun: saveRun(true); break;
            case UiAction::ImportRun:
                view.paused=true;
#ifdef __EMSCRIPTEN__
                EM_ASM({ document.getElementById('run-file').click(); },0);
#else
                saveMessage("Drop a .ashrun file onto the window, or start with --load <file>.");
#endif
                break;
            case UiAction::Restart:
                cancelRunRead();
                world.reset(); saveStatus=0; view.saveMessage.clear(); ++restartSerial; view.paused = false; view.help = false; accumulator_ = 0;
                aim = world.player.position+world.player.facing;
                renderer.resetPlayerAnimation(world.player);
                world.notify("A new descent. The same oath.",3.0f);
                break;
            case UiAction::Fullscreen:
#ifdef __EMSCRIPTEN__
            {
                EmscriptenFullscreenChangeEvent state{};
                emscripten_get_fullscreen_status(&state);
                if (state.isFullscreen) emscripten_exit_fullscreen();
                else emscripten_request_fullscreen("#game-root",EM_TRUE);
                break;
            }
#else
                ToggleFullscreen(); break;
#endif
            case UiAction::CycleHead: world.cycleEquipment(EquipmentSlot::Head); break;
            case UiAction::CycleArmor: world.cycleEquipment(EquipmentSlot::Armor); break;
            case UiAction::CycleBoots: world.cycleEquipment(EquipmentSlot::Boots); break;
            case UiAction::Heal: world.heal(); break;
            case UiAction::Attack: world.attackAt(aim); break;
            case UiAction::Dash: world.dashTo(aim); break;
            case UiAction::ZoomIn: view.zoom = std::min(1.65f,view.zoom+0.1f); break;
            case UiAction::ZoomOut: view.zoom = std::max(0.7f,view.zoom-0.1f); break;
            case UiAction::ResetZoom: view.zoom = 1; break;
            case UiAction::ImportArt:
                view.paused=true;
#ifdef __EMSCRIPTEN__
                EM_ASM({ document.getElementById('equipment-file').click(); },0);
#else
                world.notify("Drop a .ashgear file onto this window to apply equipment artwork.",6);
#endif
                break;
            case UiAction::RestoreArt:
                restoreArt();
#ifdef __EMSCRIPTEN__
                clearEquipmentInBrowser();
#endif
                break;
            case UiAction::None: break;
        }
    }
    void saveMessage(const std::string& message) { view.saveMessage=message; world.notify(message,6); }
    void saveRun(bool portable) {
        std::string error;
        const auto bytes=save::encodeRun(world,error);
        if (bytes.empty()) { saveStatus=-3; saveMessage(error); return; }
        saveStatus=3; // Pending, not a success until the storage transaction commits.
#ifdef __EMSCRIPTEN__
        if (portable) { downloadSnapshot(bytes.data(),static_cast<int>(bytes.size())); saveStatus=5; saveMessage("Run download started. Keep the .ashrun file."); }
        else { writeSnapshotInBrowser(bytes.data(),static_cast<int>(bytes.size())); saveMessage("Saving run in this browser..."); }
#else
        const save::LocalStore store=portable?save::LocalStore{"build/saves"}:save::LocalStore{};
        if (store.write(save::Slot::Run,bytes,error)) { saveStatus=1; ++saveSerial; saveMessage("Saved "+store.path(save::Slot::Run).string()); }
        else { saveStatus=-2; saveMessage(error); }
#endif
    }
    void loadRun() {
        view.paused=true;
        saveStatus=3; saveMessage("Loading saved run...");
#ifdef __EMSCRIPTEN__
        EM_ASM({ Module.snapshotStore.load(); },0);
#else
        const auto stored=save::LocalStore{}.read(save::Slot::Run,save::MaxRunBytes);
        if (!stored.error.empty()) { saveStatus=-1; saveMessage(stored.error+" Current run kept."); }
        else if (!stored.found) { saveStatus=0; saveMessage("No saved run yet. Press K to save."); }
        else restoreRun(stored.bytes.data(),stored.bytes.size());
#endif
    }
    bool restoreRun(const unsigned char* bytes,std::size_t size) {
        ++saveEventSerial;
        std::string error; auto staged=save::decodeRun(bytes,size,error);
        if (!staged) { saveStatus=-3; saveMessage(error); return false; }
        world=std::move(*staged); ++loadSerial; saveStatus=4;
        view.paused=true; view.help=false; accumulator_=0;
        aim=world.player.position+world.player.facing;
        renderer.resetPlayerAnimation(world.player);
        renderer.prepare(world,view,0);
        queuedKeys_.fill(false); leftQueued_=rightQueued_=false;
        saveMessage("Run loaded. Resume when ready; equipment artwork is unchanged."); return true;
    }
    void snapshotStatus(int status) {
        ++saveEventSerial;
        saveStatus=status;
        if (status==1) { ++saveSerial; saveMessage("Run saved in this browser. K overwrites this one slot."); }
        else if (status==0) saveMessage("No saved run in this browser yet. Press K to save.");
        else if (status==-3) saveMessage("Invalid or oversized .ashrun save. Current run kept.");
        else saveMessage("Save storage unavailable. Current run kept; export .ashrun from Pause.");
    }
    bool importArt(const unsigned char* bytes,std::size_t size) {
        ++artSerial;
        std::string error; auto project=art::decode(bytes,size,error);
        if (!project) { world.notify(error,6); return false; }
        if (!renderer.applyArt(*project)) {
            world.notify("Equipment sprites unavailable; current artwork kept. Enable sprite rendering to import.",6); return false;
        }
        artName=project->name; artHash=2166136261u;
        for (std::size_t i=0;i<size;++i) { artHash^=bytes[i]; artHash*=16777619u; }
        world.notify("Applied artwork: "+artName+". Equipment stats are unchanged.",5); return true;
    }
    void restoreArt(bool announce=true) {
        renderer.restoreArt(); artName="built-in"; artHash=0; ++artSerial;
        if (announce) world.notify("Built-in equipment artwork restored. Stats unchanged.",4);
    }
#ifndef __EMSCRIPTEN__
    void importRunFile(const char* file) {
        const auto stored=save::readFile(file,save::MaxRunBytes);
        if (!stored.error.empty() || !stored.found) {
            saveStatus=-1; saveMessage("Could not read .ashrun file (128 KiB max). Current run kept."); return;
        }
        restoreRun(stored.bytes.data(),stored.bytes.size());
    }
    void importArtFile(const char* file) {
        const int length=GetFileLength(file);
        if (length<=0 || static_cast<std::size_t>(length)>art::MaxFileBytes) { world.notify("Use a .ashgear file smaller than 50 MiB.",5); return; }
        int size=0; unsigned char* data=LoadFileData(file,&size);
        if (data) importArt(data,static_cast<std::size_t>(std::max(0,size)));
        else world.notify("Could not open the equipment project.",5);
        UnloadFileData(data);
    }
#endif
    // Raylib's held states are retained. Browser press edges are additionally
    // buffered so a down/up pair between RAF callbacks is never lost.
    void queueKey(int key) {
        // Cancel when the intent arrives, not only at the next RAF: a pending
        // load must not clear a newly buffered Restart/Save/Load key first.
        if (key==KEY_R || key==KEY_K || key==KEY_L) cancelRunRead();
        if (key>=0 && key<static_cast<int>(queuedKeys_.size())) queuedKeys_[static_cast<std::size_t>(key)] = true;
    }
    void queueMouse(int button,Vector2 position) {
        if (button==0) {
            const auto command=renderer.hitTest(position,world,view);
            if (command==UiAction::Restart || command==UiAction::SaveRun || command==UiAction::LoadRun || command==UiAction::ImportRun) cancelRunRead();
            leftQueued_ = true; leftPosition_ = position;
        }
        if (button==2) { rightQueued_ = true; rightPosition_ = position; }
    }
    bool pressed(int key) const {
        return IsKeyPressed(key) || (key>=0 && key<static_cast<int>(queuedKeys_.size()) &&
                                   queuedKeys_[static_cast<std::size_t>(key)]);
    }
    void frame() {
        const float dt = std::clamp(GetFrameTime(),0.0f,0.1f);
        renderer.prepare(world,view,dt);
        const Vector2 mouse = GetMousePosition();
        const bool inScene = renderer.sceneContains(mouse,world,view);
        if (inScene) {
            aim = renderer.screenToWorld(mouse);
            if (!world.selectedEnemy() && !world.player.attack.active && !world.player.walking && world.player.dashRemaining<=0)
                world.player.facing = normalized(aim-world.player.position,world.player.facing);
        }
        if (pressed(KEY_R)) action(UiAction::Restart);
        if (pressed(KEY_K)) action(UiAction::SaveRun);
        if (pressed(KEY_L)) action(UiAction::LoadRun);
        if (pressed(KEY_U)) action(UiAction::ImportArt);
#ifndef __EMSCRIPTEN__
        if (IsFileDropped()) {
            const FilePathList files=LoadDroppedFiles();
            if (files.count>0) {
                if (IsFileExtension(files.paths[0],".ashrun")) importRunFile(files.paths[0]);
                else importArtFile(files.paths[0]);
            }
            UnloadDroppedFiles(files);
        }
#endif
        if (pressed(KEY_I)) action(UiAction::ToggleInventory);
        if (pressed(KEY_H) || pressed(KEY_F1)) action(UiAction::ToggleHelp);
        if (pressed(KEY_F)) action(UiAction::Fullscreen);
        if (pressed(KEY_G)) view.showGrid = !view.showGrid;
        if (pressed(KEY_ESCAPE) || pressed(KEY_P)) {
            if (view.help) view.help = false;
            else if (world.player.alive() && !world.cleared()) action(UiAction::TogglePause);
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || leftQueued_) {
            const Vector2 point = leftQueued_ ? leftPosition_ : mouse;
            const UiAction clicked = renderer.hitTest(point,world,view);
            if (clicked != UiAction::None) action(clicked);
            else if (renderer.sceneContains(point,world,view)) {
                const std::uint32_t enemy = renderer.pickEnemy(point,world);
                if (enemy!=0) world.selectEnemy(enemy);
                else {
                    world.movePlayerTo(renderer.screenToWorld(point));
                    ++groundSerial;
                }
            }
        }
        const bool running = !view.paused && !view.help && world.player.alive() && !world.cleared();
        if (running) {
            if (pressed(KEY_ONE)) action(UiAction::CycleHead);
            if (pressed(KEY_TWO)) action(UiAction::CycleArmor);
            if (pressed(KEY_THREE)) action(UiAction::CycleBoots);
            if (pressed(KEY_Q)) action(UiAction::Heal);
            if (pressed(KEY_LEFT_SHIFT) || pressed(KEY_RIGHT_SHIFT)) action(UiAction::Dash);
            const bool queuedAttack = rightQueued_ && renderer.sceneContains(rightPosition_,world,view);
            if (queuedAttack) aim = renderer.screenToWorld(rightPosition_);
            if (IsKeyDown(KEY_SPACE) || pressed(KEY_SPACE) || queuedAttack ||
                (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && inScene)) world.attackAt(aim);
            if (inScene) view.zoom = std::clamp(view.zoom+GetMouseWheelMove()*0.08f,0.7f,1.65f);
        }
        constexpr float step = 1.0f/120.0f;
        if (!view.paused && !view.help) {
            accumulator_ = std::min(0.1f,accumulator_+dt);
            while (accumulator_ >= step) {
                const bool animating=world.player.alive() && !world.cleared() && world.hitStop<=0;
                world.update(step);
                renderer.animatePlayer(world.player,animating?step:0,true);
                accumulator_ -= step;
            }
        } else accumulator_ = 0; // Never catch up time spent paused or in another tab.
        // Synchronize input-only facing/attack changes even on a frame with no
        // simulation step. Presentation never writes back to the simulation.
        renderer.animatePlayer(world.player,0,!view.paused && !view.help);
        // Keep layout/camera changes (inventory, help, zoom) in sync with the
        // state rendered this frame, rather than waiting another browser RAF.
        renderer.prepare(world,view,0);
        const bool overScene = renderer.sceneContains(mouse,world,view);
        SetMouseCursor(renderer.hitTest(mouse,world,view)!=UiAction::None ||
                       (overScene && renderer.pickEnemy(mouse,world)!=0) ? MOUSE_CURSOR_POINTING_HAND :
                       (overScene ? MOUSE_CURSOR_CROSSHAIR : MOUSE_CURSOR_DEFAULT));
        renderer.draw(world,view);
        queuedKeys_.fill(false);
        leftQueued_ = rightQueued_ = false;
    }
private:
    void cancelRunRead() {
#ifdef __EMSCRIPTEN__
        EM_ASM({ if(Module.snapshotStore) Module.snapshotStore.cancel(); },0);
#endif
    }
    float accumulator_ = 0;
    std::array<bool,512> queuedKeys_{};
    bool leftQueued_ = false, rightQueued_ = false;
    Vector2 leftPosition_{}, rightPosition_{};
};
} // namespace ashfall

namespace {
std::unique_ptr<ashfall::Application> app;
void frame() { app->frame(); }
#ifdef __EMSCRIPTEN__
EM_BOOL mouseDown(int,const EmscriptenMouseEvent* event,void*) {
    if (app) app->queueMouse(event->button,{static_cast<float>(event->targetX),static_cast<float>(event->targetY)});
    return EM_FALSE; // Let Raylib's window listener maintain the held states.
}
EM_BOOL keyDown(int,const EmscriptenKeyboardEvent* event,void*) {
    if (!app || event->repeat) return EM_FALSE;
    int key = static_cast<int>(event->keyCode);
    if (std::strcmp(event->code,"Escape")==0) key = KEY_ESCAPE;
    else if (std::strcmp(event->code,"ShiftLeft")==0) key = KEY_LEFT_SHIFT;
    else if (std::strcmp(event->code,"ShiftRight")==0) key = KEY_RIGHT_SHIFT;
    else if (std::strcmp(event->code,"F1")==0) key = KEY_F1;
    app->queueKey(key);
    return EM_FALSE;
}
EM_BOOL resize(int,const EmscriptenUiEvent*,void*) {
    double width = 0, height = 0;
    if (emscripten_get_element_css_size("#canvas",&width,&height)==EMSCRIPTEN_RESULT_SUCCESS)
        SetWindowSize(std::clamp(static_cast<int>(width),1,8192),std::clamp(static_cast<int>(height),1,8192));
    return EM_FALSE;
}
EM_BOOL visibility(int,const EmscriptenVisibilityChangeEvent* event,void*) {
    if (app && event->hidden && app->world.player.alive() && !app->world.cleared()) app->view.paused = true;
    return EM_FALSE;
}
#endif
}

#ifdef __EMSCRIPTEN__
namespace { std::vector<unsigned char> artImportBytes,snapshotBytes; }
extern "C" {
EMSCRIPTEN_KEEPALIVE unsigned char* ashfall_snapshot_buffer(int size) {
    if (size<=0 || static_cast<std::size_t>(size)>ashfall::save::MaxRunBytes) return nullptr;
    snapshotBytes.resize(static_cast<std::size_t>(size)); return snapshotBytes.data();
}
EMSCRIPTEN_KEEPALIVE void ashfall_snapshot_receive(int size) {
    if (app && size>0 && static_cast<std::size_t>(size)==snapshotBytes.size()) app->restoreRun(snapshotBytes.data(),snapshotBytes.size());
    std::vector<unsigned char>().swap(snapshotBytes);
}
EMSCRIPTEN_KEEPALIVE void ashfall_snapshot_status(int status) { if (app) app->snapshotStatus(status); }
EMSCRIPTEN_KEEPALIVE unsigned char* ashfall_art_import_buffer(int size) {
    if (size<=0 || static_cast<std::size_t>(size)>ashfall::art::MaxFileBytes) return nullptr;
    artImportBytes.resize(static_cast<std::size_t>(size)); return artImportBytes.data();
}
EMSCRIPTEN_KEEPALIVE int ashfall_art_import_file(int size) {
    if (!app || size<=0 || static_cast<std::size_t>(size)!=artImportBytes.size()) return 0;
    const bool accepted=app->importArt(artImportBytes.data(),artImportBytes.size());
    std::vector<unsigned char>().swap(artImportBytes); return accepted?1:0;
}
EMSCRIPTEN_KEEPALIVE void ashfall_art_restore() { if (app) app->restoreArt(app->artName!="built-in"); }
EMSCRIPTEN_KEEPALIVE void ashfall_art_status(int status) {
    if (!app) return;
    if (status==1) app->world.notify("Equipment artwork applied and remembered in this browser. Stats unchanged.",5);
    else if (status==2) app->world.notify("Browser storage unavailable. Imported art lasts this session; keep your .ashgear file.",6);
    else if (status==5) app->world.notify("Restored for this session. Browser storage could not be cleared; it may return on reload.",6);
    else app->world.notify("Could not read equipment art. Use a .ashgear project smaller than 50 MiB.",5);
}
}
#endif

#if defined(__EMSCRIPTEN__) && defined(ASHFALL_WEB_TEST_API)
// Opt-in, read-only diagnostics for browser integration tests. No test-only game
// mutations: smoke tests interact through the exact same mouse/keyboard as a user.
extern "C" EMSCRIPTEN_KEEPALIVE const char* ashfall_get_state() {
    static char json[4096];
    const auto& world = app->world;
    const auto& player = world.player;
    const auto screen = app->renderer.worldToScreen(player.position);
    const auto camera = app->renderer.camera();
    const auto hud = app->renderer.layout().targetHud;
    const auto* selected = world.selectedEnemy();
    const auto pose = app->renderer.playerPose();
    const int n = std::snprintf(json,sizeof(json),
        "{\"saveStatus\":%d,\"saveEventSerial\":%u,\"saveSerial\":%u,\"loadSerial\":%u,\"artName\":\"%s\",\"artFrames\":%zu,\"artHash\":%u,\"artSerial\":%u,\"health\":%d,\"potions\":%d,\"armor\":%d,\"head\":%d,\"body\":%d,\"boots\":%d,"
        "\"x\":%.4f,\"y\":%.4f,\"moving\":%s,\"pathSize\":%zu,\"pathNext\":%zu,\"pathVisited\":%zu,"
        "\"kills\":%d,\"enemies\":%d,\"attack\":%s,"
        "\"dashCooldown\":%.3f,\"paused\":%s,\"help\":%s,\"inventory\":%s,\"grid\":%s,"
        "\"screenX\":%.2f,\"screenY\":%.2f,\"cameraX\":%.2f,\"cameraY\":%.2f,"
        "\"targetX\":%.2f,\"targetY\":%.2f,\"zoom\":%.4f,\"elapsed\":%.3f,"
        "\"restartSerial\":%u,\"groundSerial\":%u,\"selectedId\":%u,\"targetAction\":%d,\"selectedHealth\":%d,\"selectedMaxHealth\":%d,"
        "\"attackSerial\":%u,\"attackInterval\":%.4f,\"attackSpeed\":%.3f,\"attackCooldown\":%.4f,"
        "\"attackElapsed\":%.4f,\"attackDuration\":%.4f,\"walking\":%s,\"dashRemaining\":%.4f,"
        "\"spriteReady\":%s,\"spriteUploads\":%d,\"spriteClip\":\"%s\",\"spriteDirection\":\"%s\",\"spriteFrame\":%d,"
        "\"spriteWeaponBehind\":%s,\"spriteWalkDistance\":%.6f,\"spriteIdleSeconds\":%.6f,\"previewFrame\":%d,"
        "\"targetHud\":{\"x\":%.2f,\"y\":%.2f,\"width\":%.2f,\"height\":%.2f},\"targets\":[",
        app->saveStatus,app->saveEventSerial,app->saveSerial,app->loadSerial,app->artName.c_str(),app->renderer.customArtFrames(),app->artHash,app->artSerial,player.health,player.potions,player.equipment.armor(),player.equipment.variant(ashfall::EquipmentSlot::Head),
        player.equipment.variant(ashfall::EquipmentSlot::Armor),player.equipment.variant(ashfall::EquipmentSlot::Boots),
        static_cast<double>(player.position.x),static_cast<double>(player.position.y),player.navigation.moving()?"true":"false",
        player.navigation.waypoints.size(),player.navigation.next,world.lastPathVisited,
        world.kills,world.livingEnemies(),player.attack.active?"true":"false",static_cast<double>(player.dashCooldown),
        app->view.paused?"true":"false",app->view.help?"true":"false",app->view.inventory?"true":"false",
        app->view.showGrid?"true":"false",static_cast<double>(screen.x),static_cast<double>(screen.y),
        static_cast<double>(camera.offset.x),static_cast<double>(camera.offset.y),
        static_cast<double>(camera.target.x),static_cast<double>(camera.target.y),static_cast<double>(camera.zoom),
        static_cast<double>(world.elapsed),app->restartSerial,app->groundSerial,world.selectedEnemyId(),static_cast<int>(world.targetAction()),
        selected?selected->health:0,selected?selected->maxHealth:0,player.attack.serial,
        static_cast<double>(player.attackInterval()),static_cast<double>(player.attackSpeed()),
        static_cast<double>(player.attack.cooldown),static_cast<double>(player.attack.elapsed),static_cast<double>(player.attack.duration),
        player.walking?"true":"false",static_cast<double>(player.dashRemaining),
        app->renderer.spritesReady()?"true":"false",app->renderer.spriteUploads(),
        ashfall::character::clipInfo(pose.clip).name,ashfall::character::directionName(pose.direction),pose.frame,
        ashfall::character::weaponBehind(pose)?"true":"false",app->renderer.walkDistance(),app->renderer.idleSeconds(),
        app->renderer.previewPose().frame,static_cast<double>(hud.x),static_cast<double>(hud.y),
        static_cast<double>(hud.width),static_cast<double>(hud.height));
    std::size_t written = static_cast<std::size_t>(std::max(0,n));
    for (std::size_t i = 0; i < world.enemies.size() && written+180<sizeof(json); ++i) {
        const auto& enemy = world.enemies[i];
        const auto s = app->renderer.worldToScreen(enemy.position);
        const int count = std::snprintf(json+written,sizeof(json)-written,
            "%s{\"id\":%u,\"x\":%.3f,\"y\":%.3f,\"health\":%d,\"screenX\":%.2f,\"screenY\":%.2f,\"state\":%d}",
            i?",":"",enemy.id,static_cast<double>(enemy.position.x),static_cast<double>(enemy.position.y),enemy.health,
            static_cast<double>(s.x),static_cast<double>(s.y),static_cast<int>(enemy.state));
        written += static_cast<std::size_t>(std::max(0,count));
    }
    if (written+3<sizeof(json)) std::snprintf(json+written,sizeof(json)-written,"]}");
    return json;
}
#endif

int main(int argc,char** argv) {
#ifdef __EMSCRIPTEN__
    static_cast<void>(argc); static_cast<void>(argv);
#endif
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    int width = 1440, height = 900;
#ifdef __EMSCRIPTEN__
    width = EM_ASM_INT({ return window.innerWidth; },0);
    height = EM_ASM_INT({ return window.innerHeight; },0);
#endif
    InitWindow(width,height,"Ashfall | The Ashen Sanctum");
    if (!IsWindowReady()) {
#ifdef __EMSCRIPTEN__
        EM_ASM({ Module.onAbort("WebGL context unavailable"); },0);
#endif
        return 1;
    }
    SetTextureFilter(GetFontDefault().texture,TEXTURE_FILTER_BILINEAR);
    SetExitKey(KEY_NULL);
#ifndef __EMSCRIPTEN__
    SetWindowMinSize(640,480);
    SetTargetFPS(60);
#endif
    app = std::make_unique<ashfall::Application>();
#ifdef __EMSCRIPTEN__
    installSnapshotBridge(0); installEquipmentBridge(1);
    emscripten_set_mousedown_callback("#canvas",nullptr,EM_TRUE,mouseDown);
    emscripten_set_keydown_callback("#canvas",nullptr,EM_TRUE,keyDown);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,nullptr,EM_FALSE,resize);
    emscripten_set_visibilitychange_callback(nullptr,EM_FALSE,visibility);
    emscripten_set_main_loop(frame,0,EM_TRUE);
#else
    if (argc==3 && std::strcmp(argv[1],"--art")==0) app->importArtFile(argv[2]);
    if (argc==3 && std::strcmp(argv[1],"--load")==0) app->importRunFile(argv[2]);
    while (!WindowShouldClose()) frame();
    app.reset();
    CloseWindow();
#endif
    return 0;
}

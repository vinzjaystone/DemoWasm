#pragma once

#include "world.h"
#include "character_visual.h"
#include "raylib.h"
#include <vector>

namespace ashfall {

struct ViewState {
    bool inventory = true;
    bool help = false;
    bool paused = false;
    bool showGrid = false;
    float zoom = 1.0f;
    std::string saveMessage;
};
enum class UiAction {
    None, ToggleInventory, ToggleHelp, TogglePause, Restart, Fullscreen,
    CycleHead, CycleArmor, CycleBoots, Heal, Attack, Dash, ZoomIn, ZoomOut, ResetZoom, ImportArt, RestoreArt, SaveRun, LoadRun, ExportRun, ImportRun
};
struct Layout {
    Rectangle header{};
    Rectangle scene{};
    Rectangle targetHud{};
    Rectangle sidebar{};
    Rectangle footer{};
    Rectangle inventoryButton{};
    Rectangle helpButton{};
    Rectangle pauseButton{};
    Rectangle fullscreenButton{};
    Rectangle headSlot{}, armorSlot{}, bootsSlot{}, artImport{}, artRestore{};
    Rectangle attackButton{}, healButton{}, dashButton{};
    Rectangle zoomIn{}, zoomOut{}, resetZoom{};
    Rectangle modal{}, modalPrimary{}, modalSecondary{};
    Rectangle saveRun{},loadRun{},exportRun{},importRun{};
    bool sidebarVisible = false;
};

class Renderer {
public:
    void initializePlayer(const Player& player);
    void resetPlayerAnimation(const Player& player) { animator_.reset(player); }
    void animatePlayer(const Player& player,float dt,bool running) { animator_.update(player,dt,running); }
    character::Pose playerPose() const { return animator_.pose(); }
    character::Pose previewPose() const { return animator_.preview(); }
    bool spritesReady() const { return sprites_.ready(); }
    int spriteUploads() const { return sprites_.uploads(); }
    bool applyArt(const art::Project& project) { return sprites_.apply(project); }
    void restoreArt() { sprites_.clearCustom(); }
    std::size_t customArtFrames() const { return sprites_.customFrames(); }
    double walkDistance() const { return animator_.walkDistance(); }
    double idleSeconds() const { return animator_.idleSeconds(); }
    void prepare(const World& world, const ViewState& view, float frameDt);
    void draw(const World& world, const ViewState& view);
    UiAction hitTest(Vector2 mouse, const World& world, const ViewState& view) const;
    bool sceneContains(Vector2 mouse, const World& world, const ViewState& view) const;
    // Front-most living sprite under the cursor (body or feet), never an entity pointer.
    std::uint32_t pickEnemy(Vector2 point, const World& world) const;
    Vec2 screenToWorld(Vector2 point) const;
    Vector2 worldToScreen(Vec2 point) const;
    const Layout& layout() const { return layout_; }
    const Camera2D& camera() const { return camera_; }
    const IsoProjection& projection() const { return iso_; }

private:
    struct DrawItem {
        float depth = 0;
        int kind = 0; // 0 obstacle, 1 portal, 2 enemy, 3 player
        std::size_t index = 0;
        TileCoord tile;
    };
    IsoProjection iso_;
    Camera2D camera_{};
    Layout layout_;
    std::vector<DrawItem> drawList_;
    character::Animator animator_;
    character::Sprites sprites_;
    float time_ = 0;

    void drawDungeon(const World& world, const ViewState& view);
    void drawFloor(const World& world, const ViewState& view);
    void drawObstacle(const World& world, TileCoord tile);
    void drawPortal(const World& world);
    void drawHero(Vector2 feet, const Player& player, float scale, bool preview = false) const;
    void drawEnemy(const Enemy& enemy, bool selected, bool hovered) const;
    void drawSector(Vec2 center, Vec2 direction, float radius, float halfArc, Color fill, Color edge, float lift = 0) const;
    void drawHud(const World& world, const ViewState& view) const;
    void drawTargetHud(const World& world) const;
    void drawSidebar(const World& world) const;
    void drawMinimap(const World& world, Rectangle bounds) const;
    void drawModal(const World& world, const ViewState& view) const;
};

} // namespace ashfall

#pragma once

#include "world.h"
#include "raylib.h"
#include <vector>

namespace ashfall {

struct ViewState {
    bool inventory = true;
    bool help = false;
    bool paused = false;
    bool showGrid = false;
    float zoom = 1.0f;
};
enum class UiAction {
    None, ToggleInventory, ToggleHelp, TogglePause, Restart, Fullscreen,
    CycleHead, CycleArmor, CycleBoots, Heal, Attack, Dash, ZoomIn, ZoomOut, ResetZoom
};
struct Layout {
    Rectangle header{};
    Rectangle scene{};
    Rectangle sidebar{};
    Rectangle footer{};
    Rectangle inventoryButton{};
    Rectangle helpButton{};
    Rectangle pauseButton{};
    Rectangle fullscreenButton{};
    Rectangle headSlot{}, armorSlot{}, bootsSlot{};
    Rectangle attackButton{}, healButton{}, dashButton{};
    Rectangle zoomIn{}, zoomOut{}, resetZoom{};
    Rectangle modal{}, modalPrimary{}, modalSecondary{};
    bool sidebarVisible = false;
};

class Renderer {
public:
    void prepare(const World& world, const ViewState& view, float frameDt);
    void draw(const World& world, const ViewState& view);
    UiAction hitTest(Vector2 mouse, const World& world, const ViewState& view) const;
    bool sceneContains(Vector2 mouse, const World& world, const ViewState& view) const;
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
    float time_ = 0;

    void drawDungeon(const World& world, const ViewState& view);
    void drawFloor(const World& world, const ViewState& view);
    void drawObstacle(const World& world, TileCoord tile);
    void drawPortal(const World& world);
    void drawHero(Vector2 feet, const Player& player, float scale, bool preview = false) const;
    void drawEnemy(const Enemy& enemy) const;
    void drawSector(Vec2 center, Vec2 direction, float radius, float halfArc, Color fill, Color edge, float lift = 0) const;
    void drawHud(const World& world, const ViewState& view) const;
    void drawSidebar(const World& world) const;
    void drawMinimap(const World& world, Rectangle bounds) const;
    void drawModal(const World& world, const ViewState& view) const;
};

} // namespace ashfall

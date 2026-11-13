#include "character_animation.h"
#include <array>

namespace ashfall::character {
namespace {
constexpr std::array<ClipInfo,ClipCount> Clips{{{"idle",4,0},{"walk",8,4},{"attack",6,12}}};
constexpr std::array<const char*,DirectionCount> Names{{"S","SW","W","NW","N","NE","E","SE"}};
constexpr std::array<Vec2,DirectionCount> Directions{{{0,1},{-0.70710678f,0.70710678f},{-1,0},
    {-0.70710678f,-0.70710678f},{0,-1},{0.70710678f,-0.70710678f},{1,0},{0.70710678f,0.70710678f}}};
int loopFrame(double value, double period, int frames) {
    if (!std::isfinite(value) || value<=0) return 0;
    const double phase=std::fmod(value,period)/period;
    return std::clamp(static_cast<int>(phase*static_cast<double>(frames)),0,frames-1);
}
}
const ClipInfo& clipInfo(Clip clip) {
    const int index=static_cast<int>(clip);
    return Clips[static_cast<std::size_t>(index>=0 && index<ClipCount ? index : 0)];
}
const char* directionName(Direction direction) {
    const int index=static_cast<int>(direction);
    return index>=0 && index<DirectionCount ? Names[static_cast<std::size_t>(index)] : "S";
}
Vec2 screenDirection(Direction direction) {
    const int index=static_cast<int>(direction);
    return Directions[static_cast<std::size_t>(index>=0 && index<DirectionCount ? index : 0)];
}
Vec2 worldDirection(Direction direction) {
    const Vec2 screen=screenDirection(direction);
    return normalized({screen.y+screen.x*0.5f,screen.y-screen.x*0.5f});
}
Direction quantizeDirection(Vec2 worldFacing, Direction previous, bool hysteresis) {
    if (!finite(worldFacing) || lengthSquared(worldFacing)<0.000001f) return previous;
    const Vec2 projected=normalized({worldFacing.x-worldFacing.y,(worldFacing.x+worldFacing.y)*0.5f});
    // A five-degree dead band prevents direction-row flicker at sector edges.
    if (hysteresis && dot(projected,screenDirection(previous))>=std::cos(Pi/8+Pi/36)) return previous;
    const float angle=std::atan2(projected.y,projected.x)-Pi*0.5f;
    const int sector=static_cast<int>(std::floor(angle/(Pi*0.25f)+0.5f));
    return static_cast<Direction>((sector%DirectionCount+DirectionCount)%DirectionCount);
}
int idleFrame(double seconds) { return loopFrame(seconds,1.2,clipInfo(Clip::Idle).frames); }
int walkFrame(double traveledTiles) { return loopFrame(traveledTiles,WalkCycleTiles,clipInfo(Clip::Walk).frames); }
int attackFrame(const Attack& attack) {
    if (!std::isfinite(attack.elapsed) || attack.elapsed<Attack::HitStart) return 0;
    if (attack.elapsed<=Attack::HitEnd) {
        const float progress=(attack.elapsed-Attack::HitStart)/(Attack::HitEnd-Attack::HitStart);
        return 1+std::clamp(static_cast<int>(progress*3),0,2);
    }
    const float recovery=std::max(0.001f,attack.duration-Attack::HitEnd);
    return attack.elapsed<Attack::HitEnd+recovery*0.5f ? 4 : 5;
}
float weaponSwing(const Pose& pose) {
    constexpr std::array<float,6> Swing{{-1.25f,-1.0f,-0.2f,0.8f,1.25f,0.45f}};
    return pose.clip==Clip::Attack ? Swing[static_cast<std::size_t>(std::clamp(pose.frame,0,5))] : -0.15f;
}
Vec2 weaponScreenDirection(const Pose& pose) {
    const Vec2 swing=rotated(worldDirection(pose.direction),weaponSwing(pose));
    return normalized({swing.x-swing.y,(swing.x+swing.y)*0.5f});
}
bool weaponBehind(const Pose& pose) { return weaponScreenDirection(pose).y<-0.25f; }
void Animator::reset(const Player& player) {
    previousPosition_=player.position; initialized_=true; idleSeconds_=walkDistance_=0;
    pose_={Clip::Idle,quantizeDirection(player.facing),0};
}
void Animator::update(const Player& player, float dt, bool running) {
    if (!initialized_) reset(player);
    const float step=std::isfinite(dt) ? std::clamp(dt,0.0f,0.1f) : 0;
    const bool walking=player.alive() && player.walking && player.dashRemaining<=0;
    if (running && player.alive()) {
        idleSeconds_+=static_cast<double>(step);
        if (walking) {
            if (pose_.clip!=Clip::Walk) walkDistance_=0;
            const float traveled=distance(previousPosition_,player.position);
            // Respawns, teleports, and knockback spikes must not spin the cycle.
            if (std::isfinite(traveled) && traveled<=player.moveSpeed()*step*1.5f+0.001f)
                walkDistance_+=static_cast<double>(traveled);
        }
    }
    previousPosition_=player.position; // Rebase even while paused; never catch up frozen time.
    if (!running) return;
    const Vec2 facing=player.attack.active ? player.attack.direction : player.facing;
    pose_.direction=quantizeDirection(facing,pose_.direction,true);
    if (player.attack.active && player.alive()) {
        pose_.clip=Clip::Attack; pose_.frame=attackFrame(player.attack);
    } else if (player.dashRemaining>0 && player.alive()) {
        pose_.clip=Clip::Walk; pose_.frame=2; // Held lunge; dash afterimages are rendered separately.
    } else if (walking) {
        pose_.clip=Clip::Walk; pose_.frame=walkFrame(walkDistance_);
    } else {
        pose_.clip=Clip::Idle; pose_.frame=idleFrame(idleSeconds_);
    }
}

} // namespace ashfall::character

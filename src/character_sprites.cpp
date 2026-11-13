#include "character_sprites.h"
#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace ashfall::character {
namespace {
constexpr std::array<SheetInfo,LayerCount> Sheets{{
    {"body",20,8,56,74,1}, {"boots",24,50,48,36,3},
    {"armor",20,24,56,56,3}, {"head",30,6,36,42,3}, {"weapon",0,0,96,96,1}
}};
constexpr Pixel Clear{}, Outline{22,28,30,255}, Cloth{67,66,56,255}, Skin{193,155,111,255};
constexpr Pixel Metal{168,188,188,255}, Edge{229,232,211,255}, Gold{205,164,91,255};
Pixel gray(int value) {
    const auto c=static_cast<std::uint8_t>(std::clamp(value,0,255));
    return {c,c,c,255};
}
Pixel shade(Pixel color,float factor) {
    const auto channel=[factor](std::uint8_t c) {
        return static_cast<std::uint8_t>(std::clamp(static_cast<float>(c)*factor,0.0f,255.0f));
    };
    return {channel(color.r),channel(color.g),channel(color.b),color.a};
}

// Small deterministic, display-independent pixel rasterizer. It is used only at
// startup (and in tests), not as an alternate game loop or per-frame renderer.
class Canvas {
public:
    Frame pixels{};
    void put(int x,int y,Pixel color) {
        if (x>=0 && y>=0 && x<CellSize && y<CellSize)
            pixels[static_cast<std::size_t>(y*CellSize+x)]=color;
    }
    void line(Vec2 a,Vec2 b,float width,Pixel color) {
        const float radius=width*0.5f;
        const int x0=std::max(0,static_cast<int>(std::floor(std::min(a.x,b.x)-radius)));
        const int y0=std::max(0,static_cast<int>(std::floor(std::min(a.y,b.y)-radius)));
        const int x1=std::min(CellSize-1,static_cast<int>(std::ceil(std::max(a.x,b.x)+radius)));
        const int y1=std::min(CellSize-1,static_cast<int>(std::ceil(std::max(a.y,b.y)+radius)));
        const Vec2 delta=b-a;
        const float size=lengthSquared(delta);
        for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) {
            const Vec2 p{static_cast<float>(x)+0.5f,static_cast<float>(y)+0.5f};
            const float t=size>0 ? clamp01(dot(p-a,delta)/size) : 0;
            if (lengthSquared(p-(a+delta*t))<=radius*radius) put(x,y,color);
        }
    }
    void circle(Vec2 center,float radius,Pixel color) { line(center,center,radius*2,color); }
    void poly(std::initializer_list<Vec2> points,Pixel fill,bool outline=true) {
        if (points.size()<3) return;
        float minX=CellSize,minY=CellSize,maxX=0,maxY=0;
        for (Vec2 p : points) {
            minX=std::min(minX,p.x); minY=std::min(minY,p.y);
            maxX=std::max(maxX,p.x); maxY=std::max(maxY,p.y);
        }
        const int x0=std::max(0,static_cast<int>(std::floor(minX)));
        const int y0=std::max(0,static_cast<int>(std::floor(minY)));
        const int x1=std::min(CellSize-1,static_cast<int>(std::ceil(maxX)));
        const int y1=std::min(CellSize-1,static_cast<int>(std::ceil(maxY)));
        for (int y=y0;y<=y1;++y) for (int x=x0;x<=x1;++x) {
            const float px=static_cast<float>(x)+0.5f,py=static_cast<float>(y)+0.5f;
            bool inside=false;
            Vec2 previous=*(points.end()-1);
            for (Vec2 p : points) {
                if ((p.y>py)!=(previous.y>py) && px<(previous.x-p.x)*(py-p.y)/(previous.y-p.y)+p.x)
                    inside=!inside;
                previous=p;
            }
            if (inside) put(x,y,fill);
        }
        if (outline) {
            Vec2 previous=*(points.end()-1);
            for (Vec2 p : points) { line(previous,p,1,Outline); previous=p; }
        }
    }
    void limb(Vec2 a,Vec2 b,float width,Pixel color) {
        line(a,b,width+2,Outline); line(a,b,width,color);
        line(a+Vec2{-0.5f,-0.5f},b+Vec2{-0.5f,-0.5f},1,shade(color,1.25f));
    }
};

struct Foot { float u,v,lift; };
struct Rig {
    Pose pose;
    Vec2 forward,right,tile;
    float bob=0,lean=0,stride=0,lift=0;
    explicit Rig(Pose p) : pose(p) {
        tile=worldDirection(p.direction);
        if (p.clip==Clip::Attack) tile=rotated(tile,weaponSwing(p)*0.12f);
        forward={(tile.x-tile.y)*0.70710678f,(tile.x+tile.y)*0.35355339f};
        right={(tile.x+tile.y)*0.70710678f,(tile.y-tile.x)*0.35355339f};
        if (p.clip==Clip::Idle) {
            constexpr std::array<float,4> Breath{{0,1,0,-1}};
            bob=Breath[static_cast<std::size_t>(p.frame)];
        } else if (p.clip==Clip::Walk) {
            const float phase=static_cast<float>(p.frame)*Pi*0.25f;
            stride=std::sin(phase)*6; lift=std::cos(phase)*3;
            bob=std::abs(std::sin(phase))*1.5f; lean=1.5f;
        } else {
            constexpr std::array<float,6> Lean{{-2,-1,3,4,2,0}}, Bob{{0,1,-1,-2,-1,0}};
            lean=Lean[static_cast<std::size_t>(p.frame)]; bob=Bob[static_cast<std::size_t>(p.frame)];
        }
    }
    Vec2 at(float u,float v,float height) const {
        const float weight=clamp01(height/36);
        return Vec2{static_cast<float>(FootX),static_cast<float>(FootY)}+right*u+
            forward*(v+lean*weight)+Vec2{0,-height-bob*clamp01((height-12)/22)};
    }
    Foot foot(int side) const {
        const float s=static_cast<float>(side);
        return {s*5.5f,s*stride,std::max(0.0f,s*lift)};
    }
    Vec2 shoulder(int side) const { return at(static_cast<float>(side)*10,0,35); }
    Vec2 hand(int side) const {
        if (side==1 && pose.clip==Clip::Attack) {
            const float reach=pose.frame==0 ? -3.0f : (pose.frame<4 ? 15.0f : 9.0f);
            return at(pose.frame==0 ? 13.0f : 7.0f,reach,pose.frame==0 ? 31.0f : 26.0f);
        }
        return at(static_cast<float>(side)*13,-static_cast<float>(side)*stride*0.55f,24);
    }
    Vec2 elbow(int side) const {
        return (shoulder(side)+hand(side))*0.5f+right*(static_cast<float>(side)*2)+Vec2{0,2};
    }
    bool front() const { return forward.y>=-0.05f; }
    int farSide() const { return right.y>0 ? -1 : 1; }
};

// Vertical low-poly volumes turn with the character, not just with its sword.
void box(Canvas& c,const Rig& r,float u,float v,float halfU,float halfV,float low,float high,Pixel color) {
    const auto p=[&](float x,float y,float h) { return r.at(u+x*halfU,v+y*halfV,h); };
    if (std::abs(r.right.y)>0.001f) {
        const float side=r.right.y>0 ? 1.0f : -1.0f;
        c.poly({p(side,-1,low),p(side,1,low),p(side,1,high),p(side,-1,high)},shade(color,side>0?0.62f:0.85f));
    }
    if (std::abs(r.forward.y)>0.001f) {
        const float side=r.forward.y>0 ? 1.0f : -1.0f;
        c.poly({p(-1,side,low),p(1,side,low),p(1,side,high),p(-1,side,high)},shade(color,side>0?0.95f:0.68f));
    }
    c.poly({p(-1,-1,high),p(1,-1,high),p(1,1,high),p(-1,1,high)},shade(color,1.15f));
}
void arm(Canvas& c,const Rig& r,int side,Pixel color,float width,bool hands) {
    c.limb(r.shoulder(side),r.elbow(side),width,color);
    c.limb(r.elbow(side),r.hand(side),width-0.5f,shade(color,0.8f));
    if (hands) { c.circle(r.hand(side),2.6f,Outline); c.circle(r.hand(side),1.7f,Skin); }
}
void base(Canvas& c,const Rig& r) {
    for (int side : {r.farSide(),-r.farSide()}) {
        const Foot f=r.foot(side);
        const Vec2 knee=r.at(f.u,f.v*0.5f,10+f.lift*0.5f),heel=r.at(f.u,f.v,2+f.lift);
        c.limb(r.at(f.u,0,20),knee,5,Cloth); c.limb(knee,heel,4.5f,shade(Cloth,0.8f));
    }
    arm(c,r,r.farSide(),Cloth,4.5f,true);
    box(c,r,0,0,7,5,17,36,Cloth);
    arm(c,r,-r.farSide(),Cloth,4.5f,true);
    c.limb(r.at(0,0,36),r.at(0,0,43),5,Skin);
    const Vec2 head=r.at(0,0,47);
    c.circle(head,7.5f,Outline); c.circle(head+Vec2{-0.5f,-0.5f},6.5f,r.front()?Skin:shade(Cloth,0.65f));
    if (r.front()) {
        const Vec2 face=head+Vec2{r.forward.x*3,r.forward.y*6};
        c.line(face+r.right*-3+Vec2{0,-1},face+r.right*3+Vec2{0,-1},1.5f,Outline);
        c.circle(face+r.forward*2,1.2f,shade(Skin,1.2f));
        c.line(face+r.right*-2+Vec2{0,4},face+r.right*2+Vec2{0,4},1,shade(Skin,0.6f));
    }
}
void boots(Canvas& c,const Rig& r,int variant) {
    for (int side : {r.farSide(),-r.farSide()}) {
        const Foot f=r.foot(side);
        const float height=variant==1 ? 13.0f : 9.0f;
        const Vec2 ankle=r.at(f.u,f.v,4+f.lift),shin=r.at(f.u,f.v*0.4f,height+f.lift*0.5f);
        c.limb(shin,ankle,variant==1?6.5f:5.5f,gray(side==r.farSide()?145:205));
        box(c,r,f.u,f.v+2,3,4.5f,f.lift,f.lift+4,gray(side==r.farSide()?155:210));
        c.line(r.at(f.u-3,f.v+5.5f,f.lift),r.at(f.u+3,f.v+5.5f,f.lift),1,gray(47));
        if (variant==0) {
            for (float h : {5.0f,8.0f}) c.line(r.at(f.u-2.5f,f.v,h+f.lift),r.at(f.u+2.5f,f.v,h+f.lift),1,gray(244));
        } else if (variant==1) {
            c.line(shin+Vec2{-1,-1},ankle+Vec2{-1,0},1.5f,gray(252));
        } else {
            c.poly({shin+r.right*-3,shin+r.right*3,shin+r.forward*-6+Vec2{0,-3}},gray(225));
            c.line(ankle,ankle+r.forward*5,1,gray(255));
        }
    }
}
void cape(Canvas& c,const Rig& r,int variant) {
    const float width=variant==2 ? 13.0f : 10.0f;
    const float length=variant==2 ? 8.0f : 13.0f;
    c.poly({r.at(-9,-6,38),r.at(9,-6,38),r.at(width,-8-r.stride*0.12f,length+2),
        r.at(3,-8,length+4),r.at(-3,-8,length),r.at(-width,-8+r.stride*0.12f,length+2)},gray(variant==2?155:91));
    c.line(r.at(0,-6,36),r.at(0,-8,length+5),1,gray(variant==2?211:122));
    if (variant==2) {
        c.line(r.at(-width,-8,length+3),r.at(0,-8,length+1),1.5f,gray(232));
        c.line(r.at(0,-8,length+1),r.at(width,-8,length+3),1.5f,gray(232));
    }
}
void armor(Canvas& c,const Rig& r,int variant) {
    if (r.front()) cape(c,r,variant);
    arm(c,r,r.farSide(),gray(146),variant==1?5.5f:4.0f,false);
    box(c,r,0,0,8,5.5f,18,37,gray(variant==1?206:187));
    box(c,r,0,0,8,5.5f,17,20,gray(66));
    if (r.front()) {
        c.line(r.at(-6,5.7f,35),r.at(6,5.7f,23),variant==0?2.5f:1.5f,gray(variant==0?104:247));
        c.circle(r.at(0,5.8f,19),1.6f,gray(255));
        if (variant==1) {
            for (float height : {25.0f,30.0f}) c.line(r.at(-6,5.8f,height),r.at(6,5.8f,height),1,gray(245));
            const Vec2 emblem=r.at(0,5.8f,33);
            c.poly({emblem+Vec2{0,-3},emblem+Vec2{2,0},emblem+Vec2{0,3},emblem+Vec2{-2,0}},gray(252),false);
        }
    }
    for (int side : {r.farSide(),-r.farSide()}) {
        const float u=static_cast<float>(side)*10;
        box(c,r,u,0,variant==1?4.5f:3.5f,5,32,variant==1?40.0f:38.0f,gray(side==r.farSide()?163:218));
        if (side==-r.farSide()) arm(c,r,side,gray(172),variant==1?5.5f:4.0f,false);
        // Sleeves end at the wrist; the base hand stays visible.
        c.circle(r.hand(side),1.7f,gray(87));
    }
    if (!r.front()) cape(c,r,variant);
    if (variant==2) {
        const Vec2 clasp=r.at(7,5,36);
        c.circle(clasp,2,gray(252));
        c.line(clasp,r.at(-4,5.8f,27),1,gray(238));
    }
}
void head(Canvas& c,const Rig& r,int variant) {
    const Vec2 center=r.at(0,0,47);
    const float facing=r.forward.x;
    if (variant==0) {
        c.poly({center+Vec2{0,-12},center+Vec2{7,-7},center+Vec2{9,3},center+Vec2{7,11},
            center+Vec2{-8,11},center+Vec2{-10,2},center+Vec2{-7,-7}},gray(163));
        c.poly({center+Vec2{-1,-10},center+Vec2{5,-6},center+Vec2{7,6},center+Vec2{1,8},center+Vec2{-5,5},
            center+Vec2{-6,-5}},gray(222),false);
        if (r.front()) {
            const Vec2 face=center+Vec2{facing*5,r.forward.y*5};
            const float width=std::abs(facing)>0.85f ? 3.0f : 4.5f;
            c.poly({face+Vec2{0,-7},face+Vec2{width,-3},face+Vec2{width-1,4},face+Vec2{-width+1,4},
                face+Vec2{-width,-3}},Clear,false);
            c.line(face+Vec2{-width,-3},face+Vec2{0,-7},1.5f,gray(92));
        } else {
            c.line(center+Vec2{facing*2,-8},center+Vec2{facing*4,7},1,gray(117));
            c.line(center+Vec2{-5,8},center+Vec2{5,8},1,gray(236));
        }
        c.line(center+Vec2{-7,10},center+Vec2{7,10},2,gray(95));
    } else if (variant==1) {
        box(c,r,0,0,7.5f,6.5f,39,52,gray(203));
        c.poly({r.at(-7,-5,52),r.at(7,-5,52),r.at(4,1,57),r.at(-3,1,57)},gray(240));
        if (r.front()) {
            c.line(r.at(-6,6.8f,47),r.at(6,6.8f,47),2.2f,gray(29));
            c.line(r.at(0,6.9f,48),r.at(0,6.9f,40),1.7f,gray(255));
        } else c.line(r.at(-5,-6.8f,43),r.at(5,-6.8f,43),1,gray(100));
        c.line(r.at(0,-2,56),r.at(0,-9,59),3.0f,gray(89));
        c.line(r.at(0,-2,57),r.at(0,-8,60),1,gray(225));
    } else {
        c.line(center+Vec2{-6,-5},center+Vec2{6,-5},2,gray(245));
        for (int side : {-1,0,1}) {
            const float u=static_cast<float>(side)*5;
            const Vec2 root=r.at(u,0,53),tip=r.at(u,0,side==0?63.0f:60.0f);
            c.poly({root+Vec2{-2,0},tip,root+Vec2{2,0}},gray(side<0?251:190));
        }
        const Vec2 gem=r.at(0,5,54);
        c.circle(gem,1.5f,gray(255));
        c.line(r.at(-6,-4,52),r.at(6,-4,52),1,gray(132));
    }
}
void weapon(Canvas& c,const Rig& r) {
    const Vec2 hand=r.hand(1);
    const Vec2 direction=weaponScreenDirection(r.pose);
    const Vec2 normal{-direction.y,direction.x};
    const Vec2 tip=hand+direction*27;
    c.line(hand-direction*6,hand,4,Outline);
    c.line(hand-direction*5,hand,2,shade(Gold,0.65f));
    c.poly({hand+normal*2,hand+direction*22+normal*2,tip,hand+direction*22-normal*2,hand-normal*2},Metal);
    c.line(hand+normal*0.5f,tip-direction,1,Edge);
    c.line(hand-normal*5,hand+normal*5,3,Outline);
    c.line(hand-normal*4.5f,hand+normal*4.5f,1.5f,Gold);
    c.circle(hand-direction*6,1.6f,Gold);
    c.circle(hand,1.7f,Skin);
}
Pose validPose(Pose pose) {
    if (static_cast<int>(pose.clip)>=ClipCount) pose.clip=Clip::Idle;
    if (static_cast<int>(pose.direction)>=DirectionCount) pose.direction=Direction::South;
    pose.frame=std::clamp(pose.frame,0,clipInfo(pose.clip).frames-1);
    return pose;
}
}
const SheetInfo& sheetInfo(Layer layer) {
    const auto index=static_cast<std::size_t>(layer);
    return Sheets[index<LayerCount?index:0];
}
CellRect sourceCell(Layer layer,Pose pose,int variant) {
    pose=validPose(pose);
    const SheetInfo& info=sheetInfo(layer);
    variant=std::clamp(variant,0,info.variants-1);
    return {(clipInfo(pose.clip).offset+pose.frame)*info.cellWidth,
        (variant*DirectionCount+static_cast<int>(pose.direction))*info.cellHeight,info.cellWidth,info.cellHeight};
}
Frame makeStarterFrame(Layer layer,Pose pose,int variant) {
    pose=validPose(pose);
    variant=std::clamp(variant,0,sheetInfo(layer).variants-1);
    Canvas c;
    const Rig r(pose);
    switch (layer) {
        case Layer::Base: base(c,r); break;
        case Layer::Boots: boots(c,r,variant); break;
        case Layer::Armor: armor(c,r,variant); break;
        case Layer::Head: head(c,r,variant); break;
        case Layer::Weapon: weapon(c,r); break;
        default: break;
    }
    return c.pixels;
}
std::vector<Pixel> makeStarterSheet(Layer layer) {
    const SheetInfo& info=sheetInfo(layer);
    std::vector<Pixel> result(static_cast<std::size_t>(info.width()*info.height()));
    for (int variant=0;variant<info.variants;++variant) for (int direction=0;direction<DirectionCount;++direction)
        for (int clip=0;clip<ClipCount;++clip) for (int frame=0;frame<clipInfo(static_cast<Clip>(clip)).frames;++frame) {
            const Pose pose{static_cast<Clip>(clip),static_cast<Direction>(direction),frame};
            const Frame pixels=makeStarterFrame(layer,pose,variant);
            const CellRect cell=sourceCell(layer,pose,variant);
            for (int y=0;y<info.cellHeight;++y) {
                const auto source=pixels.begin()+(y+info.y)*CellSize+info.x;
                auto target=result.begin()+(cell.y+y)*info.width()+cell.x;
                std::copy_n(source,info.cellWidth,target);
            }
        }
    return result;
}

} // namespace ashfall::character

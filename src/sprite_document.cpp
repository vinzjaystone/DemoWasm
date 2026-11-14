#include "sprite_document.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <utility>

namespace ashfall::sprite {
namespace {
bool dimensionsValid(int width, int height) { return width>0 && height>0 && width<=MaxSide && height<=MaxSide; }
std::size_t index(const Bitmap& bitmap, Point point) {
    return static_cast<std::size_t>(point.y*bitmap.width+point.x);
}
}
std::uint32_t Pixel::rgba() const {
    return (static_cast<std::uint32_t>(r)<<24u) | (static_cast<std::uint32_t>(g)<<16u) |
           (static_cast<std::uint32_t>(b)<<8u) | static_cast<std::uint32_t>(a);
}
Bitmap::Bitmap(int w, int h) : width(w), height(h) {
    if (!dimensionsValid(w,h)) throw std::invalid_argument("Sprite dimensions must be between 1 and 128");
    pixels.resize(static_cast<std::size_t>(w*h));
}
bool Bitmap::valid() const {
    return dimensionsValid(width,height) && pixels.size()==static_cast<std::size_t>(width*height);
}
bool Bitmap::contains(Point point) const { return point.x>=0 && point.y>=0 && point.x<width && point.y<height; }
Pixel Bitmap::at(Point point) const { return contains(point) ? pixels[index(*this,point)] : Transparent; }
bool Bitmap::operator==(const Bitmap& other) const {
    return width==other.width && height==other.height && pixels==other.pixels;
}
std::optional<Pixel> parseHex(const std::string& input) {
    const std::string value = !input.empty() && input.front()=='#' ? input.substr(1) : input;
    if (value.size()!=6 && value.size()!=8) return std::nullopt;
    std::array<std::uint8_t,4> channels{{0,0,0,255}};
    for (std::size_t i=0;i<value.size();++i) {
        const char c=value[i];
        const int digit = c>='0' && c<='9' ? c-'0' : (c>='a' && c<='f' ? c-'a'+10 :
                          (c>='A' && c<='F' ? c-'A'+10 : -1));
        if (digit<0) return std::nullopt;
        const auto channel=i/2;
        channels[channel]=static_cast<std::uint8_t>(channels[channel]*16+digit);
    }
    return Pixel{channels[0],channels[1],channels[2],channels[3]};
}
std::string hex(Pixel color) {
    char result[10];
    std::snprintf(result,sizeof(result),"#%02X%02X%02X%02X",color.r,color.g,color.b,color.a);
    return result;
}
bool pngDimensions(const unsigned char* bytes, std::size_t size, int& width, int& height) {
    constexpr std::array<unsigned char,8> signature{{137,80,78,71,13,10,26,10}};
    if (!bytes || size<33 || size>MaxImportBytes || !std::equal(signature.begin(),signature.end(),bytes)) return false;
    if (bytes[8]!=0 || bytes[9]!=0 || bytes[10]!=0 || bytes[11]!=13 ||
        bytes[12]!='I' || bytes[13]!='H' || bytes[14]!='D' || bytes[15]!='R') return false;
    const auto read = [bytes](int offset) {
        std::uint32_t value=0;
        for (int i=0;i<4;++i) value=(value<<8u)|bytes[offset+i];
        return value;
    };
    const std::uint32_t w=read(16), h=read(20);
    if (w==0 || h==0 || w>static_cast<std::uint32_t>(MaxSide) || h>static_cast<std::uint32_t>(MaxSide) ||
        bytes[26]!=0 || bytes[27]!=0 || bytes[28]>1) return false;
    width=static_cast<int>(w); height=static_cast<int>(h);
    return true;
}
std::vector<Point> linePoints(Point from, Point to) {
    std::vector<Point> result;
    const auto bounded = [](Point p) { return p.x>=0 && p.y>=0 && p.x<MaxSide && p.y<MaxSide; };
    if (!bounded(from) || !bounded(to)) return result;
    int dx=std::abs(to.x-from.x), dy=-std::abs(to.y-from.y), error=dx+dy;
    const int sx=from.x<to.x ? 1 : -1, sy=from.y<to.y ? 1 : -1;
    result.reserve(static_cast<std::size_t>(std::max(dx,-dy)+1));
    while (true) {
        result.push_back(from);
        if (from==to) break;
        const int twice=error*2;
        if (twice>=dy) { error+=dy; from.x+=sx; }
        if (twice<=dx) { error+=dx; from.y+=sy; }
    }
    return result;
}
Bitmap example() {
    Bitmap result;
    const Pixel outline{29,38,42,255}, teal{104,177,157,255}, darkTeal{56,104,100,255};
    const Pixel gold{211,171,104,255}, cream{238,226,185,255}, iron{151,175,173,255};
    const Pixel leather{129,93,65,255};
    const auto block = [&result](int x,int y,int width,int height,Pixel color) {
        for (int row=y;row<y+height;++row) for (int col=x;col<x+width;++col)
            result.pixels[index(result,{col,row})]=color;
    };
    block(8,28,17,2,{13,18,22,100});
    block(12,5,7,1,outline); block(10,6,11,3,outline); block(9,9,13,7,outline);
    block(12,6,7,2,teal); block(11,8,9,7,teal); block(18,9,3,7,darkTeal);
    block(12,9,6,5,outline); block(13,10,4,2,cream); block(13,12,4,1,gold);
    block(8,16,16,3,outline); block(10,16,11,9,outline);
    block(10,16,4,3,teal); block(18,16,4,3,darkTeal); block(12,16,7,7,darkTeal);
    block(12,17,4,5,teal); block(11,23,10,2,leather); block(15,23,2,2,gold);
    block(11,25,4,4,outline); block(17,25,4,4,outline);
    block(12,25,3,4,leather); block(17,25,3,4,leather);
    block(7,19,4,4,outline); block(8,19,2,3,cream); block(21,19,3,3,cream);
    block(23,10,2,12,outline); block(23,11,1,9,iron); block(24,10,1,9,cream);
    block(21,21,6,1,gold); block(23,22,2,3,leather);
    return result;
}
Document::Document(Bitmap initial) : bitmap_(std::move(initial)) {
    if (!bitmap_.valid()) throw std::invalid_argument("Invalid sprite bitmap");
}
bool Document::dirty() const {
    return revision_!=savedRevision_ || (before_ && !(before_->bitmap==bitmap_));
}
void Document::markSaved() { commitStroke(); savedRevision_=revision_; }
bool Document::replace(Bitmap bitmap) {
    if (!bitmap.valid()) return false;
    bitmap_=std::move(bitmap); before_.reset(); undo_.clear(); redo_.clear();
    revision_=nextRevision_++; savedRevision_=revision_; ++stamp_;
    return true;
}
bool Document::recover(Bitmap bitmap) {
    if (!replace(std::move(bitmap))) return false;
    savedRevision_=0; return true;
}
bool Document::paste(Bitmap bitmap) {
    if (!bitmap.valid() || bitmap.width!=bitmap_.width || bitmap.height!=bitmap_.height) return false;
    commitStroke(); beginStroke(); bitmap_=std::move(bitmap); ++stamp_;
    return commitStroke();
}
void Document::beginStroke() { if (!before_) before_=Snapshot{bitmap_,revision_}; }
bool Document::commitStroke() {
    if (!before_) return false;
    if (before_->bitmap==bitmap_) { before_.reset(); return false; }
    undo_.push_back(std::move(*before_)); before_.reset();
    if (undo_.size()>HistoryLimit) undo_.pop_front();
    redo_.clear(); revision_=nextRevision_++;
    return true;
}
void Document::cancelStroke() {
    if (!before_) return;
    bitmap_=std::move(before_->bitmap); revision_=before_->revision; before_.reset(); ++stamp_;
}
bool Document::write(Point point, Pixel color, bool mirror) {
    if (!bitmap_.contains(point)) return false;
    bool changed=false;
    for (int i=0;i<(mirror?2:1);++i) {
        const Point p{i==0?point.x:bitmap_.width-1-point.x,point.y};
        Pixel& dest=bitmap_.pixels[index(bitmap_,p)];
        if (dest!=color) { dest=color; changed=true; ++stamp_; }
    }
    return changed;
}
bool Document::paintLine(Point from, Point to, Pixel color, bool mirror) {
    if (!bitmap_.contains(from) || !bitmap_.contains(to)) return false;
    beginStroke();
    bool changed=false;
    for (Point p : linePoints(from,to)) changed=write(p,color,mirror)||changed;
    return changed;
}
bool Document::rectangle(Point from, Point to, Pixel color, bool mirror) {
    if (!bitmap_.contains(from) || !bitmap_.contains(to)) return false;
    bool changed=paintLine(from,{to.x,from.y},color,mirror);
    changed=paintLine({to.x,from.y},to,color,mirror)||changed;
    changed=paintLine(to,{from.x,to.y},color,mirror)||changed;
    return paintLine({from.x,to.y},from,color,mirror)||changed;
}
bool Document::fillRegion(Point seed, Pixel color) {
    if (!bitmap_.contains(seed)) return false;
    const Pixel previous=bitmap_.at(seed);
    if (previous==color) return false;
    std::vector<Point> open{seed};
    write(seed,color,false); // Mark on enqueue, bounding the queue by pixel count.
    while (!open.empty()) {
        const Point p=open.back(); open.pop_back();
        for (Point next : {Point{p.x+1,p.y},{p.x-1,p.y},{p.x,p.y+1},{p.x,p.y-1}}) {
            if (bitmap_.contains(next) && bitmap_.at(next)==previous) {
                write(next,color,false); open.push_back(next);
            }
        }
    }
    return true;
}
bool Document::floodFill(Point seed, Pixel color, bool mirror) {
    if (!bitmap_.contains(seed)) return false;
    beginStroke();
    fillRegion(seed,color);
    if (mirror) fillRegion({bitmap_.width-1-seed.x,seed.y},color);
    return commitStroke();
}
bool Document::clear() {
    beginStroke();
    std::fill(bitmap_.pixels.begin(),bitmap_.pixels.end(),Transparent); ++stamp_;
    return commitStroke();
}
bool Document::flipHorizontal() {
    beginStroke();
    for (int y=0;y<bitmap_.height;++y) for (int x=0;x<bitmap_.width/2;++x)
        std::swap(bitmap_.pixels[index(bitmap_,{x,y})],bitmap_.pixels[index(bitmap_,{bitmap_.width-1-x,y})]);
    ++stamp_; return commitStroke();
}
bool Document::flipVertical() {
    beginStroke();
    for (int y=0;y<bitmap_.height/2;++y) for (int x=0;x<bitmap_.width;++x)
        std::swap(bitmap_.pixels[index(bitmap_,{x,y})],bitmap_.pixels[index(bitmap_,{x,bitmap_.height-1-y})]);
    ++stamp_; return commitStroke();
}
bool Document::rotateClockwise() {
    beginStroke();
    Bitmap result(bitmap_.height,bitmap_.width);
    for (int y=0;y<bitmap_.height;++y) for (int x=0;x<bitmap_.width;++x)
        result.pixels[index(result,{bitmap_.height-1-y,x})]=bitmap_.at({x,y});
    bitmap_=std::move(result); ++stamp_; return commitStroke();
}
bool Document::undo() {
    commitStroke();
    if (undo_.empty()) return false;
    redo_.push_back({std::move(bitmap_),revision_});
    bitmap_=std::move(undo_.back().bitmap); revision_=undo_.back().revision; undo_.pop_back(); ++stamp_;
    return true;
}
bool Document::redo() {
    commitStroke();
    if (redo_.empty()) return false;
    undo_.push_back({std::move(bitmap_),revision_});
    bitmap_=std::move(redo_.back().bitmap); revision_=redo_.back().revision; redo_.pop_back(); ++stamp_;
    return true;
}

} // namespace ashfall::sprite

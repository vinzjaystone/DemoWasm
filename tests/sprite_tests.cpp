#include "sprite_document.h"
#include <array>
#include <climits>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ashfall::sprite;
namespace {
int assertions=0;
#define CHECK(...) do { ++assertions; if (!(__VA_ARGS__)) throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": "+#__VA_ARGS__); } while (false)
constexpr Pixel Red{220,60,40,255},Blue{40,100,220,255},Soft{100,180,160,90};
void bitmapAndHex() {
    const Bitmap bitmap(16,24);
    CHECK(bitmap.valid() && bitmap.pixels.size()==384);
    CHECK(bitmap.at({-1,0})==Transparent && bitmap.at({16,0})==Transparent);
    CHECK(!bitmap.contains({INT_MAX,INT_MIN}));
    bool threw=false;
    try { const Bitmap bad(129,1); } catch (const std::invalid_argument&) { threw=true; }
    CHECK(threw);
    CHECK(parseHex("#DC3C28")==Red);
    CHECK(parseHex("dc3c28ff")==Red);
    CHECK(parseHex(hex(Soft))==Soft);
    CHECK(!parseHex("FFFFF") && !parseHex("#GG0000") && !parseHex("123456789"));
    CHECK(Red.rgba()==0xdc3c28ffu);
    Bitmap corrupt;
    corrupt.pixels.clear();
    CHECK(!corrupt.valid());
    Document document;
    CHECK(!document.replace(std::move(corrupt)) && document.bitmap().valid());
}
void linesAndTransactions() {
    Document document(Bitmap(16,16));
    const auto blank=document.bitmap();
    document.beginStroke();
    for (int x=0;x<16;++x) document.paintLine({x,0},{x,15},Red);
    CHECK(document.dirty() && document.editing() && document.undoDepth()==0);
    CHECK(document.commitStroke() && document.undoDepth()==1);
    for (Pixel p : document.bitmap().pixels) CHECK(p==Red);
    document.markSaved();
    CHECK(!document.dirty());
    CHECK(document.undo() && document.bitmap()==blank && document.dirty());
    CHECK(document.redo() && !document.dirty());
    document.paintLine({0,0},{15,15},Blue);
    document.cancelStroke();
    CHECK(!document.dirty() && document.bitmap().at({0,0})==Red);
    document.paintLine({0,0},{15,15},Red);
    CHECK(!document.commitStroke() && document.undoDepth()==1);
    CHECK(!document.paintLine({-1,0},{15,15},Blue));
    CHECK(!document.paintLine({0,0},{INT_MAX,INT_MIN},Blue));
    CHECK(linePoints({INT_MIN,0},{INT_MAX,0}).empty());
    CHECK(document.undo());
    document.paintLine({0,0},{15,15},Blue); document.commitStroke();
    CHECK(document.redoDepth()==0); // Editing branches invalidate redo, selection changes don't.
    for (int i=0;i<16;++i) CHECK(document.bitmap().at({i,i})==Blue);
    for (int x=0;x<8;++x) for (int y=0;y<8;++y) {
        const auto points=linePoints({x,y},{7-x,7-y});
        CHECK(points.front()==Point{x,y} && points.back()==Point{7-x,7-y});
        CHECK(points.size()<=8);
        for (Point point : points) CHECK(point.x>=0 && point.y>=0 && point.x<8 && point.y<8);
    }
}
void fillAndMirror() {
    Document document(Bitmap(9,7));
    document.rectangle({1,1},{7,5},Red); document.commitStroke();
    CHECK(document.floodFill({2,2},Blue));
    CHECK(document.bitmap().at({3,3})==Blue && document.bitmap().at({0,0})==Transparent);
    CHECK(document.bitmap().at({1,1})==Red);
    const auto count=document.undoDepth();
    CHECK(!document.floodFill({2,2},Blue) && document.undoDepth()==count);
    CHECK(!document.floodFill({INT_MAX,0},Blue));
    document.paintLine({0,0},{0,6},Soft,true); document.commitStroke();
    for (int y=0;y<7;++y) CHECK(document.bitmap().at({0,y})==Soft && document.bitmap().at({8,y})==Soft);
    document.paintLine({4,2},{4,4},Soft,true); document.commitStroke();
    CHECK(document.bitmap().at({4,3})==Soft);
    CHECK(document.clear());
    for (Pixel p : document.bitmap().pixels) CHECK(p==Transparent);
    CHECK(document.undo() && document.bitmap().at({4,3})==Soft);
    Document big(Bitmap(128,128));
    CHECK(big.floodFill({0,0},Red));
    for (Pixel p : big.bitmap().pixels) CHECK(p==Red);
    CHECK(big.undo());
    for (Pixel p : big.bitmap().pixels) CHECK(p==Transparent);
}
void transformsAndSavePoints() {
    Document document(Bitmap(3,2));
    document.paintLine({0,0},{0,0},Red); document.paintLine({2,1},{2,1},Soft); document.commitStroke();
    document.markSaved(); const auto original=document.bitmap();
    CHECK(document.flipHorizontal());
    CHECK(document.bitmap().at({2,0})==Red && document.bitmap().at({0,1})==Soft);
    CHECK(document.undo() && !document.dirty() && document.bitmap()==original);
    CHECK(document.flipVertical());
    CHECK(document.bitmap().at({0,1})==Red);
    CHECK(document.undo());
    CHECK(document.rotateClockwise());
    CHECK(document.bitmap().width==2 && document.bitmap().height==3);
    CHECK(document.bitmap().at({1,0})==Red && document.bitmap().at({0,2})==Soft);
    CHECK(document.undo() && document.bitmap()==original && !document.dirty());
    for (int i=0;i<4;++i) document.rotateClockwise();
    CHECK(document.bitmap()==original); // Pixel-perfect, including alpha.
    CHECK(document.replace(Bitmap(16,16)));
    CHECK(!document.dirty() && document.undoDepth()==0 && document.redoDepth()==0);
    CHECK(!document.undo() && !document.redo());
    Document symmetry(Bitmap(1,1));
    CHECK(!symmetry.rotateClockwise() && !symmetry.flipHorizontal() && !symmetry.flipVertical());
}
void historyBound() {
    Document document(Bitmap(128,128));
    for (int i=0;i<1000;++i) {
        document.paintLine({i%128,(i/128)%128},{i%128,(i/128)%128},i%2==0?Red:Blue);
        document.commitStroke();
        CHECK(document.undoDepth()<=Document::HistoryLimit);
        CHECK(document.bitmap().valid());
    }
    CHECK(document.undoDepth()==64);
    for (int i=0;i<64;++i) CHECK(document.undo());
    CHECK(!document.undo() && document.redoDepth()==64);
    for (int i=0;i<64;++i) CHECK(document.redo());
    CHECK(!document.redo() && document.undoDepth()==64);
}
void pngEnvelope() {
    std::array<unsigned char,33> png{{137,80,78,71,13,10,26,10,0,0,0,13,'I','H','D','R',0,0,0,16,0,0,0,8,8,6,0,0,0,0,0,0,0}};
    int w=0,h=0;
    CHECK(pngDimensions(png.data(),png.size(),w,h) && w==16 && h==8);
    CHECK(!pngDimensions(nullptr,33,w,h));
    for (std::size_t size=0;size<33;++size) CHECK(!pngDimensions(png.data(),size,w,h));
    CHECK(!pngDimensions(png.data(),MaxImportBytes+1,w,h));
    png[16]=255; CHECK(!pngDimensions(png.data(),png.size(),w,h)); png[16]=0;
    png[19]=129; CHECK(!pngDimensions(png.data(),png.size(),w,h)); png[19]=16;
    png[23]=0; CHECK(!pngDimensions(png.data(),png.size(),w,h)); png[23]=8;
    png[12]='X'; CHECK(!pngDimensions(png.data(),png.size(),w,h)); png[12]='I';
    png[28]=2; CHECK(!pngDimensions(png.data(),png.size(),w,h));
    const Bitmap starter=example(); CHECK(starter.valid());
    bool opaque=false,transparent=false,partial=false;
    for (Pixel p : starter.pixels) { opaque=opaque||p.a==255; transparent=transparent||p.a==0; partial=partial||(p.a>0 && p.a<255); }
    CHECK(opaque && transparent && partial);
}
}
int main() {
    const std::array<std::pair<const char*,void(*)()>,6> tests{{
        {"RGBA bitmaps, bounds and hex colors",bitmapAndHex},{"gapless lines, strokes, undo/redo and save points",linesAndTransactions},
        {"bounded flood fill, transparency and mirror",fillAndMirror},{"lossless transforms and rectangular sprites",transformsAndSavePoints},
        {"bounded 64-entry history",historyBound},{"PNG envelope validation and procedural starter",pngEnvelope}}};
    int failures=0;
    for (const auto& test : tests) {
        try { test.second(); std::cout<<"[PASS] "<<test.first<<'\n'; }
        catch (const std::exception& error) { ++failures; std::cerr<<"[FAIL] "<<test.first<<": "<<error.what()<<'\n'; }
    }
    std::cout<<tests.size()<<" suites, "<<assertions<<" assertions, "<<failures<<" failures\n";
    return failures==0?0:1;
}

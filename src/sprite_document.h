#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace ashfall::sprite {

struct Pixel {
    std::uint8_t r = 0, g = 0, b = 0, a = 0;
    bool operator==(Pixel other) const { return r==other.r && g==other.g && b==other.b && a==other.a; }
    bool operator!=(Pixel other) const { return !(*this==other); }
    std::uint32_t rgba() const;
};
struct Point {
    int x = 0, y = 0;
    bool operator==(Point other) const { return x==other.x && y==other.y; }
};
constexpr int MaxSide = 128;
constexpr std::size_t MaxImportBytes = 8u*1024u*1024u;
constexpr Pixel Transparent{};

// Owned, tightly packed RGBA8 pixels; independent of Raylib, file I/O, and UI.
struct Bitmap {
    int width = 32, height = 32;
    std::vector<Pixel> pixels;
    explicit Bitmap(int w = 32, int h = 32);
    bool valid() const;
    bool contains(Point point) const;
    Pixel at(Point point) const;
    bool operator==(const Bitmap& other) const;
};
std::optional<Pixel> parseHex(const std::string& value);
std::string hex(Pixel color);
Bitmap example(); // Procedural starter artwork, not an external asset.
// Validate the PNG header/dimensions before handing untrusted bytes to a decoder.
bool pngDimensions(const unsigned char* bytes, std::size_t size, int& width, int& height);
std::vector<Point> linePoints(Point from, Point to); // Bounded sprite coordinates only.

class Document {
public:
    static constexpr std::size_t HistoryLimit = 64;
    explicit Document(Bitmap initial = Bitmap{});
    const Bitmap& bitmap() const { return bitmap_; }
    std::uint64_t stamp() const { return stamp_; }
    std::uint64_t revision() const { return revision_; }
    std::size_t undoDepth() const { return undo_.size(); }
    std::size_t redoDepth() const { return redo_.size(); }
    bool editing() const { return before_.has_value(); }
    bool dirty() const;
    void markSaved();
    bool replace(Bitmap bitmap); // Fresh document; clears history and the dirty flag.
    bool paste(Bitmap bitmap); // Same-sized pixel replacement as one undoable edit.

    // A drag is one transaction, irrespective of the number of sampled points.
    // Out-of-bounds points are ignored; the UI clips captured drags to the edge.
    void beginStroke();
    bool commitStroke();
    void cancelStroke();
    bool paintLine(Point from, Point to, Pixel color, bool mirror = false);
    bool rectangle(Point from, Point to, Pixel color, bool mirror = false);
    bool floodFill(Point seed, Pixel color, bool mirror = false);
    bool clear();
    bool flipHorizontal();
    bool flipVertical();
    bool rotateClockwise(); // Rectangular imports swap width/height, without resampling.
    bool undo();
    bool redo();

private:
    struct Snapshot { Bitmap bitmap; std::uint64_t revision = 0; };
    Bitmap bitmap_;
    std::deque<Snapshot> undo_, redo_;
    std::optional<Snapshot> before_;
    std::uint64_t revision_ = 1, savedRevision_ = 1, nextRevision_ = 2, stamp_ = 1;
    bool write(Point point, Pixel color, bool mirror);
    bool fillRegion(Point seed, Pixel color);
};

} // namespace ashfall::sprite

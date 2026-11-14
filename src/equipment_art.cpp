#include "equipment_art.h"
#include <algorithm>
#include <array>

namespace ashfall::art {
namespace {
constexpr std::array<unsigned char,8> Magic{{'A','S','H','G','E','A','R',0}};
constexpr std::size_t HeaderSize=37, FrameBytes=sizeof(character::Frame);
std::uint32_t checksum(const unsigned char* data,std::size_t size) {
    std::uint32_t value=2166136261u;
    for (std::size_t i=0;i<size;++i) { value^=data[i]; value*=16777619u; }
    return value;
}
void u16(std::vector<unsigned char>& out,std::uint16_t v) {
    out.push_back(static_cast<unsigned char>(v&255)); out.push_back(static_cast<unsigned char>(v>>8u));
}
void u32(std::vector<unsigned char>& out,std::uint32_t v) {
    for (unsigned i=0;i<4;++i) out.push_back(static_cast<unsigned char>((v>>(8u*i))&255));
}
std::uint16_t read16(const unsigned char* b) { return static_cast<std::uint16_t>(b[0]|(static_cast<unsigned>(b[1])<<8u)); }
std::uint32_t read32(const unsigned char* b) {
    std::uint32_t v=0; for (unsigned i=0;i<4;++i) v|=static_cast<std::uint32_t>(b[i])<<(8u*i); return v;
}
void rgba(std::vector<unsigned char>& out,character::Pixel p) { out.insert(out.end(),{p.r,p.g,p.b,p.a}); }
}
bool Key::valid() const {
    return static_cast<std::size_t>(slot)<3 && variant>=0 && variant<3 &&
        static_cast<int>(pose.clip)<character::ClipCount && static_cast<int>(pose.direction)<character::DirectionCount &&
        pose.frame>=0 && pose.frame<character::clipInfo(pose.clip).frames;
}
character::Layer layer(EquipmentSlot slot) {
    return slot==EquipmentSlot::Head?character::Layer::Head:(slot==EquipmentSlot::Boots?character::Layer::Boots:character::Layer::Armor);
}
character::Frame starter(Key key) { return character::makeStarterFrame(layer(key.slot),key.pose,key.variant); }
sprite::Bitmap bitmap(const character::Frame& frame) {
    sprite::Bitmap result(character::CellSize,character::CellSize);
    for (std::size_t i=0;i<frame.size();++i) result.pixels[i]={frame[i].r,frame[i].g,frame[i].b,frame[i].a};
    return result;
}
character::Frame pixels(const sprite::Bitmap& image) {
    character::Frame result{};
    if (!image.valid() || image.width!=character::CellSize || image.height!=character::CellSize) return result;
    for (std::size_t i=0;i<result.size();++i) result[i]={image.pixels[i].r,image.pixels[i].g,image.pixels[i].b,image.pixels[i].a};
    return result;
}
bool validName(const std::string& name) {
    return !name.empty() && name.size()<=28 && std::all_of(name.begin(),name.end(),[](char c) {
        return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-';
    });
}
const character::Frame* Project::find(Key key) const {
    const auto it=frames_.find(key); return it==frames_.end()?nullptr:it->second.get();
}
character::Frame Project::frame(Key key) const { const auto* p=find(key); return p?*p:starter(key); }
bool Project::set(Key key,character::Frame frame) {
    if (!key.valid()) return false;
    const auto* old=find(key);
    if (old && *old==frame) return false;
    if (frame==starter(key)) return frames_.erase(key)>0;
    frames_[key]=std::make_shared<const character::Frame>(std::move(frame)); return true;
}
bool Project::operator==(const Project& other) const {
    if (name!=other.name || frames_.size()!=other.frames_.size()) return false;
    auto b=other.frames_.begin();
    for (const auto& a : frames_) {
        if (a.first!=b->first || (a.second!=b->second && *a.second!=*b->second)) return false;
        ++b;
    }
    return true;
}
bool hasSignature(const unsigned char* data,std::size_t size) {
    return data && size>=Magic.size() && std::equal(Magic.begin(),Magic.end(),data);
}
std::vector<unsigned char> encode(const Project& project) {
    if (!validName(project.name) || project.frames().size()>MaxFrames) return {};
    std::vector<unsigned char> payload;
    for (const auto& entry : project.frames()) {
        if (!entry.first.valid() || !entry.second) return {};
        std::vector<unsigned char> runs;
        const auto& frame=*entry.second;
        for (std::size_t i=0;i<frame.size();) {
            std::size_t end=i+1;
            while (end<frame.size() && frame[end]==frame[i]) ++end;
            u16(runs,static_cast<std::uint16_t>(end-i)); rgba(runs,frame[i]); i=end;
        }
        const bool rle=runs.size()<FrameBytes;
        const auto& k=entry.first;
        payload.insert(payload.end(),{static_cast<unsigned char>(k.slot),static_cast<unsigned char>(k.variant),
            static_cast<unsigned char>(k.pose.clip),static_cast<unsigned char>(k.pose.direction),
            static_cast<unsigned char>(k.pose.frame),static_cast<unsigned char>(rle?1:0)});
        u32(payload,static_cast<std::uint32_t>(rle?runs.size():FrameBytes));
        if (rle) payload.insert(payload.end(),runs.begin(),runs.end());
        else for (auto p : frame) rgba(payload,p);
    }
    std::vector<unsigned char> out(Magic.begin(),Magic.end());
    u16(out,1); u16(out,1); // File and procedural catalog/recipe versions.
    u16(out,character::CellSize); u16(out,character::FootX); u16(out,character::FootY);
    out.insert(out.end(),{8,4,8,6,3,3});
    u32(out,static_cast<std::uint32_t>(project.frames().size())); u32(out,static_cast<std::uint32_t>(payload.size()));
    u32(out,checksum(payload.data(),payload.size())); out.push_back(static_cast<unsigned char>(project.name.size()));
    out.insert(out.end(),project.name.begin(),project.name.end()); out.insert(out.end(),payload.begin(),payload.end());
    return out.size()<=MaxFileBytes?out:std::vector<unsigned char>{};
}
std::optional<Project> decode(const unsigned char* data,std::size_t size,std::string& error) {
    const auto fail=[&error](const char* message)->std::optional<Project> { error=message; return std::nullopt; };
    if (!hasSignature(data,size) || size<HeaderSize || size>MaxFileBytes) return fail("Not a valid .ashgear project (50 MiB maximum).");
    if (read16(data+8)!=1 || read16(data+10)!=1) return fail("Unsupported equipment project/catalog version.");
    constexpr std::array<unsigned char,6> counts{{8,4,8,6,3,3}};
    if (read16(data+12)!=96 || read16(data+14)!=48 || read16(data+16)!=76 || !std::equal(counts.begin(),counts.end(),data+18))
        return fail("Equipment dimensions, foot pivot, or animation metadata do not match this game.");
    const std::size_t count=read32(data+24),payloadSize=read32(data+28),nameSize=data[36];
    if (count>MaxFrames || nameSize==0 || nameSize>28 || nameSize>size-HeaderSize) return fail("Invalid project count or name.");
    const std::size_t begin=HeaderSize+nameSize;
    if (payloadSize!=size-begin || checksum(data+begin,payloadSize)!=read32(data+32)) return fail("Truncated or corrupt equipment project.");
    Project project; project.name.assign(reinterpret_cast<const char*>(data+HeaderSize),nameSize);
    if (!validName(project.name)) return fail("Invalid equipment project name.");
    std::size_t cursor=begin;
    std::optional<Key> previous;
    for (std::size_t i=0;i<count;++i) {
        if (size-cursor<10) return fail("Truncated equipment frame header.");
        Key key{static_cast<EquipmentSlot>(data[cursor]),data[cursor+1],
            {static_cast<character::Clip>(data[cursor+2]),static_cast<character::Direction>(data[cursor+3]),data[cursor+4]}};
        const int encoding=data[cursor+5]; const std::size_t length=read32(data+cursor+6); cursor+=10;
        if (!key.valid() || (previous && !(*previous<key))) return fail("Invalid or duplicate equipment frame identity.");
        if (length>size-cursor || length>FrameBytes || (encoding!=0 && encoding!=1)) return fail("Invalid equipment frame size/encoding.");
        character::Frame frame{};
        if (encoding==0) {
            if (length!=FrameBytes) return fail("Raw equipment frame must be 96 x 96 RGBA.");
            for (std::size_t p=0;p<frame.size();++p) frame[p]={data[cursor+p*4],data[cursor+p*4+1],data[cursor+p*4+2],data[cursor+p*4+3]};
        } else {
            if (length==0 || length%6!=0) return fail("Invalid equipment RLE frame.");
            std::size_t written=0;
            for (std::size_t p=0;p<length;p+=6) {
                const std::size_t run=read16(data+cursor+p);
                if (run==0 || run>frame.size()-written) return fail("Equipment RLE expands beyond a frame.");
                const character::Pixel value{data[cursor+p+2],data[cursor+p+3],data[cursor+p+4],data[cursor+p+5]};
                std::fill_n(frame.begin()+static_cast<std::ptrdiff_t>(written),run,value); written+=run;
            }
            if (written!=frame.size()) return fail("Equipment RLE does not fill the frame.");
        }
        project.set(key,std::move(frame)); cursor+=length; previous=key;
    }
    if (cursor!=size) return fail("Unexpected trailing equipment data.");
    error.clear(); return project;
}
Workspace::Workspace() : document_(bitmap(starter(key_))) { syncedStamp_=document_.stamp(); }
void Workspace::sync() {
    if (document_.editing() || syncedStamp_==document_.stamp()) return;
    project_.set(key_,pixels(document_.bitmap())); document_.markSaved(); syncedStamp_=document_.stamp();
    dirty_=project_!=saved_;
}
bool Workspace::select(Key key) {
    if (!key.valid()) return false;
    document_.commitStroke(); sync(); key_=key; document_.replace(bitmap(project_.frame(key_)));
    syncedStamp_=document_.stamp(); return true;
}
void Workspace::open(Project project) {
    project_=std::move(project); saved_=project_; dirty_=false;
    key_=project_.frames().empty()?Key{}:project_.frames().begin()->first;
    document_.replace(bitmap(project_.frame(key_))); syncedStamp_=document_.stamp();
}
void Workspace::markSaved() { document_.commitStroke(); sync(); saved_=project_; dirty_=false; document_.markSaved(); }
bool Workspace::rename(const std::string& name) {
    if (!validName(name) || name==project_.name) return false;
    project_.name=name; dirty_=project_!=saved_; return true;
}
void Workspace::resetFrame() { document_.paste(bitmap(starter(key_))); sync(); }

} // namespace ashfall::art

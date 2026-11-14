#pragma once

#include "equipment_art.h"
#include "snapshot_codec.h"

namespace ashfall::save {
constexpr snapshot::Magic DraftMagic{{'A','S','H','D','R','A','F','T'}};
constexpr std::size_t MaxDraftBytes=art::MaxFileBytes+128;
// Recovery preserves pixels, all edited gear frames, selection, name and drawing
// settings. History/GPU resources and the exported save point are not persisted.
struct Draft {
    bool equipment=false;
    std::string name="untitled";
    sprite::Bitmap image{32,32};
    art::Project project;
    art::Key key;
    sprite::Pixel foreground{218,178,112,255};
    bool grid=true,mirror=false,references=true,playing=true;
};
snapshot::Bytes encodeDraft(const Draft& draft,std::string& error);
std::optional<Draft> decodeDraft(const unsigned char* bytes,std::size_t size,std::string& error);

// Small scheduling policy independent of RAF, storage or Raylib: coalesce bursts
// for 0.6s, checkpoint continuous drawing within 2s, retry failed I/O only when
// explicitly requested. It never changes a document's saved/dirty revision.
class AutosaveClock {
public:
    void changed() { if (!pending_) age_=0; pending_=true; quiet_=0; }
    void advance(float dt);
    bool due() const { return pending_ && (quiet_>=0.6f || age_>=2); }
    bool pending() const { return pending_; }
    void sent() { pending_=false; age_=quiet_=0; }
private:
    bool pending_=false;
    float age_=0,quiet_=0;
};
} // namespace ashfall::save

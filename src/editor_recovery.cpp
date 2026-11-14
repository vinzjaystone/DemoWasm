#include "editor_recovery.h"
#include <algorithm>
#include <utility>

namespace ashfall::save {
snapshot::Bytes encodeDraft(const Draft& draft,std::string& error) {
    error.clear();
    if (!art::validName(draft.name) || (draft.equipment && (!draft.key.valid() || draft.project.name!=draft.name)) ||
        (!draft.equipment && !draft.image.valid())) { error="Invalid editor recovery state. Export your artwork."; return {}; }
    snapshot::Writer out;
    out.boolean(draft.equipment); out.name(draft.name);
    for (auto channel:{draft.foreground.r,draft.foreground.g,draft.foreground.b,draft.foreground.a}) out.u8(channel);
    for (bool setting:{draft.grid,draft.mirror,draft.references,draft.playing}) out.boolean(setting);
    if (draft.equipment) {
        out.u8(static_cast<unsigned char>(draft.key.slot)); out.u8(static_cast<unsigned char>(draft.key.variant));
        out.u8(static_cast<unsigned char>(draft.key.pose.clip)); out.u8(static_cast<unsigned char>(draft.key.pose.direction));
        out.u8(static_cast<unsigned char>(draft.key.pose.frame));
        auto project=art::encode(draft.project);
        if (project.empty()) { error="Recovery project exceeds limits. Save a .ashgear backup."; return {}; }
        out.blob(project);
    } else {
        out.u32(static_cast<std::uint32_t>(draft.image.width)); out.u32(static_cast<std::uint32_t>(draft.image.height));
        for (auto pixel:draft.image.pixels) { out.u8(pixel.r); out.u8(pixel.g); out.u8(pixel.b); out.u8(pixel.a); }
    }
    auto bytes=snapshot::wrap(DraftMagic,out.bytes);
    if (bytes.size()>MaxDraftBytes) { error="Editor recovery exceeds storage limits. Export your artwork."; return {}; }
    return bytes;
}
std::optional<Draft> decodeDraft(const unsigned char* bytes,std::size_t size,std::string& error) {
    error.clear();
    const auto fail=[&error]()->std::optional<Draft> {
        error="Corrupt or incompatible recovery draft. Current artwork kept; export it before discarding the draft.";
        return std::nullopt;
    };
    auto in=snapshot::unwrap(bytes,size,DraftMagic,MaxDraftBytes);
    if (!in.ok) return fail();
    Draft draft;
    draft.equipment=in.boolean(); draft.name=in.name();
    if (!art::validName(draft.name)) return fail();
    draft.foreground={in.u8(),in.u8(),in.u8(),in.u8()};
    draft.grid=in.boolean(); draft.mirror=in.boolean(); draft.references=in.boolean(); draft.playing=in.boolean();
    if (draft.equipment) {
        const auto slot=in.u8(),variant=in.u8(),clip=in.u8(),direction=in.u8(),frame=in.u8();
        draft.key={static_cast<EquipmentSlot>(slot),variant,{static_cast<character::Clip>(clip),static_cast<character::Direction>(direction),frame}};
        if (!in.ok || !draft.key.valid()) return fail();
        const auto projectBytes=in.blob(art::MaxFileBytes);
        if (!in.done()) return fail();
        auto project=art::decode(projectBytes.data(),projectBytes.size(),error);
        if (!project || project->name!=draft.name) return fail();
        draft.project=std::move(*project);
    } else {
        const auto width=in.u32(),height=in.u32();
        if (!in.ok || width==0 || height==0 || width>sprite::MaxSide || height>sprite::MaxSide ||
            in.size-in.cursor!=static_cast<std::size_t>(width)*height*4) return fail();
        draft.image=sprite::Bitmap{static_cast<int>(width),static_cast<int>(height)};
        for (auto& pixel:draft.image.pixels) pixel={in.u8(),in.u8(),in.u8(),in.u8()};
    }
    if (!in.done()) return fail();
    return draft;
}
void AutosaveClock::advance(float dt) {
    if (!pending_ || !std::isfinite(dt) || dt<=0) return;
    age_=std::min(2.0f,age_+dt); quiet_=std::min(2.0f,quiet_+dt);
}
} // namespace ashfall::save

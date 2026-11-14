#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace ashfall::snapshot {
using Bytes=std::vector<unsigned char>;
using Magic=std::array<unsigned char,8>;
constexpr std::size_t HeaderBytes=20;
inline std::uint32_t checksum(const unsigned char* bytes,std::size_t size) {
    std::uint32_t result=2166136261u;
    for (std::size_t i=0;i<size;++i) { result^=bytes[i]; result*=16777619u; }
    return result;
}
struct Writer {
    Bytes bytes;
    void u8(unsigned char value) { bytes.push_back(value); }
    void u32(std::uint32_t value) { for (unsigned i=0;i<4;++i) u8(static_cast<unsigned char>((value>>(i*8u))&255)); }
    void boolean(bool value) { u8(value?1:0); }
    void number(float value) {
        static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559,"Snapshots require IEEE binary32");
        std::uint32_t bits=0; std::memcpy(&bits,&value,4); u32(bits);
    }
    void blob(const Bytes& data) { u32(static_cast<std::uint32_t>(data.size())); bytes.insert(bytes.end(),data.begin(),data.end()); }
    void name(const std::string& value) { u8(static_cast<unsigned char>(value.size())); bytes.insert(bytes.end(),value.begin(),value.end()); }
};
struct Reader {
    const unsigned char* data=nullptr;
    std::size_t size=0,cursor=0;
    bool ok=true;
    bool take(std::size_t n) { if (!ok || cursor>size || n>size-cursor || (!data && n!=0)) { ok=false; return false; } return true; }
    unsigned char u8() { if (!take(1)) return 0; return data[cursor++]; }
    std::uint32_t u32() {
        if (!take(4)) return 0;
        std::uint32_t value=0; for (unsigned i=0;i<4;++i) value|=static_cast<std::uint32_t>(data[cursor++])<<(8u*i); return value;
    }
    bool boolean() { const auto value=u8(); if (value>1) ok=false; return value!=0; }
    float number() { const auto bits=u32(); float value=0; std::memcpy(&value,&bits,4); if (!std::isfinite(value)) ok=false; return value; }
    Bytes blob(std::size_t maximum) {
        const std::size_t n=u32(); if (n>maximum || !take(n)) { ok=false; return {}; }
        Bytes result(data+cursor,data+cursor+n); cursor+=n; return result;
    }
    std::string name() {
        const std::size_t n=u8(); if (n==0 || n>28 || !take(n)) { ok=false; return {}; }
        std::string result(reinterpret_cast<const char*>(data+cursor),n); cursor+=n; return result;
    }
    bool done() const { return ok && cursor==size; }
};
inline Bytes wrap(Magic magic,const Bytes& payload) {
    Writer out; out.bytes.insert(out.bytes.end(),magic.begin(),magic.end());
    out.u32(1); out.u32(static_cast<std::uint32_t>(payload.size())); out.u32(checksum(payload.data(),payload.size()));
    out.bytes.insert(out.bytes.end(),payload.begin(),payload.end()); return out.bytes;
}
inline bool signature(const unsigned char* bytes,std::size_t size,Magic magic) {
    return bytes && size>=magic.size() && std::memcmp(bytes,magic.data(),magic.size())==0;
}
inline Reader unwrap(const unsigned char* bytes,std::size_t size,Magic magic,std::size_t maximum) {
    if (!signature(bytes,size,magic) || size<HeaderBytes || size>maximum) return {nullptr,0,0,false};
    Reader header{bytes+8,12};
    const auto version=header.u32(),length=header.u32(),sum=header.u32();
    if (version!=1 || length!=size-HeaderBytes || checksum(bytes+HeaderBytes,length)!=sum) return {nullptr,0,0,false};
    return {bytes+HeaderBytes,length};
}
} // namespace ashfall::snapshot

#pragma once

#ifndef __EMSCRIPTEN__
#include "snapshot_codec.h"
#include <filesystem>
#include <utility>

namespace ashfall::save {
enum class Slot { Run, Editor };
struct Stored {
    snapshot::Bytes bytes;
    std::string error;
    bool found=false;
};
std::filesystem::path dataDirectory();
Stored readFile(const std::filesystem::path& file,std::size_t maximum);
// Native files only, deliberately absent from the filesystem-free WASM build.
// Writes stage next to the destination then atomically replace it; a failed
// stage must not truncate a previous save. The caller still validates its codec.
class LocalStore {
public:
    explicit LocalStore(std::filesystem::path directory=dataDirectory()) : directory_(std::move(directory)) {}
    Stored read(Slot slot,std::size_t maximum) const;
    bool write(Slot slot,const snapshot::Bytes& bytes,std::string& error) const;
    bool clear(Slot slot,std::string& error) const;
    std::filesystem::path path(Slot slot) const;
private:
    std::filesystem::path directory_;
};
} // namespace ashfall::save
#endif

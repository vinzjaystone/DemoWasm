#include "local_store.h"
#ifndef __EMSCRIPTEN__
#include "editor_recovery.h"
#include "run_save.h"
#include <atomic>
#include <cstdlib>
#include <fstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace ashfall::save {
namespace {
std::filesystem::path environment(const char* key) {
    const char* value=std::getenv(key); return value && *value?std::filesystem::path{value}:std::filesystem::path{};
}
std::size_t limit(Slot slot) { return slot==Slot::Run?MaxRunBytes:MaxDraftBytes; }
}
std::filesystem::path dataDirectory() {
    if (const auto path=environment("ASHFALL_DATA_DIR"); !path.empty()) return path;
#ifdef _WIN32
    if (const auto path=environment("LOCALAPPDATA"); !path.empty()) return path/"Ashfall";
    if (const auto path=environment("APPDATA"); !path.empty()) return path/"Ashfall";
#elif defined(__APPLE__)
    if (const auto path=environment("HOME"); !path.empty()) return path/"Library"/"Application Support"/"Ashfall";
#else
    if (const auto path=environment("XDG_DATA_HOME"); path.is_absolute()) return path/"ashfall";
    if (const auto path=environment("HOME"); !path.empty()) return path/".local"/"share"/"ashfall";
#endif
    return std::filesystem::path{"build"}/"saves";
}
std::filesystem::path LocalStore::path(Slot slot) const { return directory_/(slot==Slot::Run?"run.ashrun":"editor.ashdraft"); }
Stored readFile(const std::filesystem::path& file,std::size_t maximum) {
    Stored result;
    std::error_code code;
    const bool exists=std::filesystem::exists(file,code);
    if (code) { result.error="Could not access local save storage."; return result; }
    if (!exists) return result;
    result.found=true;
    const auto size=std::filesystem::file_size(file,code);
    if (code || size==0 || size>maximum) { result.error="Local save is unreadable or exceeds size limits."; return result; }
    std::ifstream input(file,std::ios::binary);
    result.bytes.resize(static_cast<std::size_t>(size));
    input.read(reinterpret_cast<char*>(result.bytes.data()),static_cast<std::streamsize>(result.bytes.size()));
    // Reject a file concurrently truncated or grown; decoding is staged too.
    if (!input || input.peek()!=std::char_traits<char>::eof()) { result.bytes.clear(); result.error="Local save changed or could not be read."; }
    return result;
}
Stored LocalStore::read(Slot slot,std::size_t maximum) const { return readFile(path(slot),std::min(maximum,limit(slot))); }
bool LocalStore::write(Slot slot,const snapshot::Bytes& bytes,std::string& error) const {
    error.clear();
    if (bytes.empty() || bytes.size()>limit(slot)) { error="Local save exceeds size limits."; return false; }
    std::error_code code;
    std::filesystem::create_directories(directory_,code);
    if (code) { error="Could not create local save directory."; return false; }
    static std::atomic<unsigned long> serial{0};
#ifdef _WIN32
    const auto process=GetCurrentProcessId();
#else
    const auto process=getpid();
#endif
    const auto destination=path(slot);
    auto temporary=destination;
    temporary+=".tmp-"+std::to_string(process)+"-"+std::to_string(++serial);
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        output.flush();
        if (!output) error="Could not write local save. Previous file kept.";
        output.close();
        if (!output && error.empty()) error="Could not close local save. Previous file kept.";
    }
    if (error.empty()) {
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            error="Could not replace local save. Previous file kept.";
#else
        std::filesystem::rename(temporary,destination,code);
        if (code) error="Could not replace local save. Previous file kept.";
#endif
    }
    std::filesystem::remove(temporary,code);
    return error.empty();
}
bool LocalStore::clear(Slot slot,std::string& error) const {
    error.clear(); std::error_code code;
    std::filesystem::remove(path(slot),code);
    if (code) error="Could not clear local recovery. It may return next time.";
    return !code;
}
} // namespace ashfall::save
#endif

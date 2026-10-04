#pragma once
// The game mods (.o2r) this server shares (docs/superpowers/specs/2026-10-03-coop-mods-o2r-design.md): read once at
// start (size and SHA-256), announced in "welcome" (Handlers/SessionHandlers.cpp) and served by chunks
// (Handlers/O2rHandlers.cpp). A game must have loaded all of them to enter the world (Handlers/WorldHandlers.cpp).
#include "ServerConfig.h"

#include "common/O2r.h"

#include <filesystem>
#include <string>
#include <vector>

namespace coop::server {

class O2rStore {
  public:
    // Reads the files config names (what was loaded before is forgotten); warnings gets what was left out and why.
    void Load(const O2rConfig& config, std::string* warnings);
    const std::vector<o2r::Entry>& Entries() const {
        return mEntries;
    }
    bool Empty() const {
        return mEntries.empty();
    }
    json ListJson() const {
        return o2r::ListToJson(mEntries);
    }
    // Up to max bytes of file index from offset (fewer at its end). False if it cannot be read.
    bool ReadChunk(size_t index, uint64_t offset, size_t max, std::vector<uint8_t>& out) const;
    // A world_enter whose "o2r" names the hash of every file (always true when the server shares none).
    bool Accepts(const json& worldEnter) const;
    // dir holds some .o2r (the scripts' folder: they were probably meant for the players).
    static bool HasO2rFiles(const std::string& dir);

  private:
    std::vector<o2r::Entry> mEntries;
    std::vector<std::filesystem::path> mPaths;
};

} // namespace coop::server

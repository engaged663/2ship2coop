#pragma once
// A whole saved world as its files hold it (world.json + players/): what an import or a restored backup puts in place
// of the current world (SharedWorld::Replace) and what the converter tool writes. The one place that knows
// world.json's shape: SharedWorld saves and loads it through these too.
#include "PlayerStore.h"
#include "WorldStore.h"

#include "common/Events.h"
#include "common/SaveImport.h"

#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

struct WorldImage {
    FieldSet fields;        // the world now
    FieldSet start;         // the world at the start of its cycle (the moon brings it back)
    int cycle = 1;          // its number; the players' cycles below are relative to it
    uint32_t clockAbs = 0;  // coop::clock units
    bool inverted = false;  // Inverted Song of Time
    bool frozen = false;    // /freezetime
    std::vector<PlayerRecord> players;
};

// world.json: the store's fields and copy, plus the clock {abs, inv, frozen}.
json WorldFileJson(const WorldStore& store, uint32_t abs, bool inverted, bool frozen);
// The other way (lenient, see WorldStore::ParseFieldsLenient); players are left empty.
bool ParseWorldFile(const json& saved, WorldImage& out, std::vector<std::string>* warnings, std::string* err);
// world.json and the players/ folder next to it (a backup's, or a server folder).
bool LoadWorldImage(const std::string& worldPath, const std::string& playersDir, WorldImage& out,
                    std::vector<std::string>* warnings, std::string* err);
// Writes both; the players already in playersDir are set aside as .old (PlayerStore::Clear), never deleted.
bool SaveWorldImage(const WorldImage& image, const std::string& worldPath, const std::string& playersDir,
                    std::string* err);
// A converted base-game save: cycle 1, one player (nick) with their data and their cycle-start copy.
WorldImage ImageFromImport(const save::Imported& imported, const std::string& nick);

} // namespace coop::server

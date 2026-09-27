#pragma once
// players/<nick in lowercase>.json (a Windows device name such as "con" gets "con-.json"): each player's own data,
// opaque to the server (inventory, position...), the world cycle it belongs to and a copy of it from the start of
// the cycle (the moon brings it back).
#include "common/Events.h"

#include <map>
#include <set>
#include <string>

namespace coop::server {

struct PlayerRecord {
    std::string nick;    // as last seen (the file name is the lowercase key)
    json inv;            // null = never uploaded
    int cycle = 0;       // world cycle of the last upload
    json start;          // the first upload of startCycle
    int startCycle = -1;
};

class PlayerStore {
  public:
    explicit PlayerStore(std::string dir); // "" = memory only (tests)

    void LoadAll(std::string* warnings);
    const PlayerRecord* Get(const std::string& nick) const; // case-insensitive
    size_t Count() const {
        return mRecords.size();
    }
    void Upload(const std::string& nick, const json& inv, int worldCycle);
    // The moon: whoever has a copy from oldCycle gets it back as their data for newCycle.
    void RestoreCycleStart(int oldCycle, int newCycle);
    // Their data is from an earlier cycle: the game applies the end-of-cycle rules when entering.
    bool IsStale(const std::string& nick, int worldCycle) const;
    bool SaveAll(std::string* err); // changed records only
    void Clear();                   // a new world: forget everyone (files renamed to .old, never deleted)

  private:
    std::string PathOf(const std::string& key) const;

    std::string mDir;
    std::map<std::string, PlayerRecord> mRecords;
    std::set<std::string> mDirty;
};

} // namespace coop::server

#pragma once
// One mod's own data (coop.storage.*): a JSON object kept in <dataDir>/<mod>.json. ModHost owns one per mod, loads
// it on first use and writes it when it changed (every couple of seconds, when the mod unloads, when the server
// stops).
#include "common/Events.h"

#include <string>
#include <vector>

namespace coop::server {

class ModStorage {
  public:
    explicit ModStorage(std::string path); // "" = memory only (tests)

    bool Unreadable() const { // the file was there but it is not a JSON object: the mod starts without data
        return mUnreadable;
    }
    json Get(const std::string& key) const; // null when missing
    void Set(const std::string& key, const json& value); // null removes it
    bool Remove(const std::string& key);
    std::vector<std::string> Keys() const; // sorted
    bool Dirty() const {
        return mDirty;
    }
    bool Save(std::string* err); // nothing to do when it did not change

  private:
    std::string mPath;
    json mData = json::object();
    bool mDirty = false;
    bool mUnreadable = false;
};

} // namespace coop::server

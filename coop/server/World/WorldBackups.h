#pragma once
// Copies of the saved world: backups/<YYYY-MM-DD_hh-mm-ss>[_NN]-<reason>/ with world.json and players/*.json, taken by
// SharedWorld (at start when the world changed, every backupMinutes, before the moon, a reset, an import or a restore)
// and by /backup ("manual"). The newest `keep` are kept; manual ones are never removed by themselves.
#include <string>
#include <vector>

namespace coop::server {

struct BackupInfo {
    std::string name;   // the folder's name: "2026-10-05_14-30-00-auto"
    std::string reason; // "start", "auto", "moon", "sot", "restart", "ending", "import", "restore", "manual"
};

class WorldBackups {
  public:
    WorldBackups(std::string dir, int keep); // dir "" = off (a server without files, the tests in memory)

    bool Enabled() const {
        return !mDir.empty();
    }
    void SetKeep(int keep);
    // Copies world.json and the players' files as they are on disk now into a new folder (name gets it), then drops
    // the oldest automatic copies beyond `keep`. False + err when off or world.json cannot be copied.
    bool Make(const std::string& worldPath, const std::string& playersDir, const std::string& reason,
              std::string* name, std::string* err);
    std::vector<BackupInfo> List() const; // newest first
    std::string Newest() const;           // "" = none
    // "ultima"/"última"/"last"/"latest" = the newest; else a backup's whole name, or the start of exactly one.
    // "" = no such backup.
    std::string Resolve(const std::string& nameOrLast) const;
    std::string WorldFile(const std::string& name) const;  // <dir>/<name>/world.json
    std::string PlayersDir(const std::string& name) const; // <dir>/<name>/players
    // world.json has the same bytes as the newest copy's: nothing new to keep (a server restarted without changes).
    bool SameAsNewest(const std::string& worldPath) const;

  private:
    void Prune();

    std::string mDir;
    int mKeep;
};

} // namespace coop::server

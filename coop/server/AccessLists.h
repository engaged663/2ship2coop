#pragma once
// bans.json and ops.json. Empty paths keep everything in memory (tests).
#include <string>
#include <vector>

namespace coop::server {

struct BanEntry {
    std::string nick; // matched case-insensitively
    std::string ip;   // matched exactly ("" = none)
    std::string reason;
    std::string date;
};

class AccessLists {
  public:
    AccessLists(std::string bansPath = "", std::string opsPath = "");

    void Load();

    bool IsBanned(const std::string& nick, const std::string& ip, std::string* reason) const;
    void Ban(const std::string& nick, const std::string& ip, const std::string& reason);
    // Removes every entry whose nick (case-insensitive) or ip equals nickOrIp. False if none matched.
    bool Unban(const std::string& nickOrIp);
    const std::vector<BanEntry>& Bans() const;

    bool IsOp(const std::string& nick) const;
    bool AddOp(const std::string& nick);    // false if already op
    bool RemoveOp(const std::string& nick); // false if not op

  private:
    void SaveBans() const;
    void SaveOps() const;

    std::string mBansPath;
    std::string mOpsPath;
    std::vector<BanEntry> mBans;
    std::vector<std::string> mOps;
};

} // namespace coop::server

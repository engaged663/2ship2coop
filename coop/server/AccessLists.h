#pragma once
// bans.json and ops.json (created empty on first run). Empty paths keep everything in memory (tests).
// An op is a nick AND the IP it was granted from: nicks are not authenticated, so anyone could take one.
#include <string>
#include <vector>

namespace coop::server {

struct OpEntry {
    std::string nick; // matched case-insensitively
    std::string ip;   // matched exactly
};

struct BanEntry {
    std::string nick; // matched case-insensitively
    std::string ip;   // matched exactly ("" = none)
    std::string reason;
    std::string date;
};

class AccessLists {
  public:
    AccessLists(std::string bansPath = "", std::string opsPath = "");

    // Entries with wrong types are skipped; warnings (if given) says which.
    void Load(std::string* warnings = nullptr);

    bool IsBanned(const std::string& nick, const std::string& ip, std::string* reason) const;
    void Ban(const std::string& nick, const std::string& ip, const std::string& reason);
    // Removes every entry whose nick (case-insensitive) or ip equals nickOrIp. False if none matched.
    bool Unban(const std::string& nickOrIp);
    const std::vector<BanEntry>& Bans() const;

    bool IsOp(const std::string& nick, const std::string& ip) const;
    bool AddOp(const std::string& nick, const std::string& ip); // re-binds the IP; false if nothing changed
    bool RemoveOp(const std::string& nick);                     // false if not op

  private:
    void SaveBans() const;
    void SaveOps() const;

    std::string mBansPath;
    std::string mOpsPath;
    std::vector<BanEntry> mBans;
    std::vector<OpEntry> mOps;
};

} // namespace coop::server

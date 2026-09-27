#include "AccessLists.h"

#include "common/Text.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <ctime>
#include <fstream>

namespace coop::server {

namespace {

std::string Today() {
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &local);
    return buf;
}

nlohmann::json ReadJson(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        std::ofstream(path) << "[]\n"; // first run: create it so the owner can find and edit it
        return nlohmann::json::array();
    }
    return nlohmann::json::parse(in, nullptr, false);
}

// Missing field = "", wrong type = false (the whole entry is skipped).
bool ReadField(const nlohmann::json& obj, const char* key, std::string& out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
        out.clear();
        return true;
    }
    if (!it->is_string()) {
        return false;
    }
    out = it->get<std::string>();
    return true;
}

void Warn(std::string* warnings, const std::string& text) {
    if (warnings != nullptr) {
        *warnings += (warnings->empty() ? "" : "; ") + text;
    }
}

} // namespace

AccessLists::AccessLists(std::string bansPath, std::string opsPath)
    : mBansPath(std::move(bansPath)), mOpsPath(std::move(opsPath)) {
}

void AccessLists::Load(std::string* warnings) {
    mBans.clear();
    mOps.clear();
    if (!mBansPath.empty()) {
        nlohmann::json bans = ReadJson(mBansPath);
        if (!bans.is_array()) {
            Warn(warnings, mBansPath + " no es una lista JSON; se ignora");
        } else {
            for (auto& b : bans) {
                BanEntry entry;
                if (!b.is_object() || !ReadField(b, "nick", entry.nick) || !ReadField(b, "ip", entry.ip) ||
                    !ReadField(b, "reason", entry.reason) || !ReadField(b, "date", entry.date) ||
                    (entry.nick.empty() && entry.ip.empty())) {
                    Warn(warnings, mBansPath + ": se ignora una entrada inválida");
                    continue;
                }
                mBans.push_back(entry);
            }
        }
    }
    if (!mOpsPath.empty()) {
        nlohmann::json ops = ReadJson(mOpsPath);
        if (!ops.is_array()) {
            Warn(warnings, mOpsPath + " no es una lista JSON; se ignora");
        } else {
            for (auto& o : ops) {
                OpEntry entry;
                if (!o.is_object() || !ReadField(o, "nick", entry.nick) || !ReadField(o, "ip", entry.ip) ||
                    entry.nick.empty() || entry.ip.empty()) {
                    Warn(warnings, mOpsPath + ": se ignora una entrada sin nick o sin IP (usa 'op <jugador>' "
                                              "en la consola con el jugador conectado)");
                    continue;
                }
                mOps.push_back(entry);
            }
        }
    }
}

bool AccessLists::IsBanned(const std::string& nick, const std::string& ip, std::string* reason) const {
    for (const auto& ban : mBans) {
        bool nickMatch = !ban.nick.empty() && EqualsIgnoreCase(ban.nick, nick);
        bool ipMatch = !ban.ip.empty() && ban.ip == ip;
        if (nickMatch || ipMatch) {
            if (reason != nullptr) {
                *reason = ban.reason;
            }
            return true;
        }
    }
    return false;
}

void AccessLists::Ban(const std::string& nick, const std::string& ip, const std::string& reason) {
    mBans.push_back({ nick, ip, reason, Today() });
    SaveBans();
}

bool AccessLists::Unban(const std::string& nickOrIp) {
    size_t before = mBans.size();
    std::erase_if(mBans, [&](const BanEntry& b) {
        return (!b.nick.empty() && EqualsIgnoreCase(b.nick, nickOrIp)) || (!b.ip.empty() && b.ip == nickOrIp);
    });
    if (mBans.size() == before) {
        return false;
    }
    SaveBans();
    return true;
}

const std::vector<BanEntry>& AccessLists::Bans() const {
    return mBans;
}

bool AccessLists::IsOp(const std::string& nick, const std::string& ip) const {
    for (const auto& op : mOps) {
        if (EqualsIgnoreCase(op.nick, nick) && op.ip == ip) {
            return true;
        }
    }
    return false;
}

bool AccessLists::AddOp(const std::string& nick, const std::string& ip) {
    for (auto& op : mOps) {
        if (EqualsIgnoreCase(op.nick, nick)) {
            if (op.ip == ip) {
                return false;
            }
            op.ip = ip; // same person from a new address: the console decides
            SaveOps();
            return true;
        }
    }
    mOps.push_back({ nick, ip });
    SaveOps();
    return true;
}

bool AccessLists::RemoveOp(const std::string& nick) {
    size_t before = mOps.size();
    std::erase_if(mOps, [&](const OpEntry& op) { return EqualsIgnoreCase(op.nick, nick); });
    if (mOps.size() == before) {
        return false;
    }
    SaveOps();
    return true;
}

void AccessLists::SaveBans() const {
    if (mBansPath.empty()) {
        return;
    }
    nlohmann::json out = nlohmann::json::array();
    for (const auto& b : mBans) {
        out.push_back({ { "nick", b.nick }, { "ip", b.ip }, { "reason", b.reason }, { "date", b.date } });
    }
    std::ofstream(mBansPath) << out.dump(4) << '\n';
}

void AccessLists::SaveOps() const {
    if (mOpsPath.empty()) {
        return;
    }
    nlohmann::json out = nlohmann::json::array();
    for (const auto& op : mOps) {
        out.push_back({ { "nick", op.nick }, { "ip", op.ip } });
    }
    std::ofstream(mOpsPath) << out.dump(4) << '\n';
}

} // namespace coop::server

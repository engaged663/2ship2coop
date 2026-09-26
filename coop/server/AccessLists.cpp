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
        return nullptr;
    }
    return nlohmann::json::parse(in, nullptr, false);
}

} // namespace

AccessLists::AccessLists(std::string bansPath, std::string opsPath)
    : mBansPath(std::move(bansPath)), mOpsPath(std::move(opsPath)) {
}

void AccessLists::Load() {
    mBans.clear();
    mOps.clear();
    if (!mBansPath.empty()) {
        nlohmann::json bans = ReadJson(mBansPath);
        if (bans.is_array()) {
            for (auto& b : bans) {
                if (b.is_object()) {
                    mBans.push_back({ b.value("nick", ""), b.value("ip", ""), b.value("reason", ""),
                                      b.value("date", "") });
                }
            }
        }
    }
    if (!mOpsPath.empty()) {
        nlohmann::json ops = ReadJson(mOpsPath);
        if (ops.is_array()) {
            for (auto& o : ops) {
                if (o.is_string()) {
                    mOps.push_back(o.get<std::string>());
                }
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

bool AccessLists::IsOp(const std::string& nick) const {
    for (const auto& op : mOps) {
        if (EqualsIgnoreCase(op, nick)) {
            return true;
        }
    }
    return false;
}

bool AccessLists::AddOp(const std::string& nick) {
    if (IsOp(nick)) {
        return false;
    }
    mOps.push_back(nick);
    SaveOps();
    return true;
}

bool AccessLists::RemoveOp(const std::string& nick) {
    size_t before = mOps.size();
    std::erase_if(mOps, [&](const std::string& op) { return EqualsIgnoreCase(op, nick); });
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
    std::ofstream(mOpsPath) << nlohmann::json(mOps).dump(4) << '\n';
}

} // namespace coop::server

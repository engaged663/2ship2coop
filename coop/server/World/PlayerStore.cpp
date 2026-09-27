#include "PlayerStore.h"

#include "JsonFile.h"

#include "common/Text.h"

#include <filesystem>
#include <system_error>
#include <vector>

namespace coop::server {

namespace {

bool IsObjectOrNull(const json& value) {
    return value.is_object() || value.is_null();
}

bool ParseRecord(const json& saved, PlayerRecord& out) {
    if (!saved.is_object()) {
        return false;
    }
    auto inv = saved.find("inv");
    auto start = saved.find("start");
    if (inv == saved.end() || !IsObjectOrNull(*inv) || (start != saved.end() && !IsObjectOrNull(*start))) {
        return false;
    }
    out.nick = GetString(saved, "nick");
    out.inv = *inv;
    out.cycle = (int)GetInt(saved, "cycle", 0);
    out.start = start != saved.end() ? *start : json(nullptr);
    out.startCycle = (int)GetInt(saved, "startCycle", -1);
    return true;
}

void AddWarning(std::string* warnings, const std::string& text) {
    if (warnings != nullptr) {
        *warnings += (warnings->empty() ? "" : "; ") + text;
    }
}

} // namespace

PlayerStore::PlayerStore(std::string dir) : mDir(std::move(dir)) {
}

std::string PlayerStore::PathOf(const std::string& key) const {
    return (std::filesystem::path(mDir) / (key + ".json")).string();
}

void PlayerStore::LoadAll(std::string* warnings) {
    mRecords.clear();
    mDirty.clear();
    if (mDir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(mDir, ec);
    for (const auto& entry : std::filesystem::directory_iterator(mDir, ec)) {
        std::error_code fileEc;
        if (!entry.is_regular_file(fileEc) || entry.path().extension() != ".json") {
            continue;
        }
        json saved;
        PlayerRecord record;
        if (!LoadJsonFile(entry.path().string(), saved, nullptr) || !ParseRecord(saved, record)) {
            AddWarning(warnings, "players/" + entry.path().filename().string() + " no es válido; se ignora");
            continue;
        }
        std::string key = ToLower(entry.path().stem().string());
        if (record.nick.empty()) {
            record.nick = key;
        }
        mRecords[key] = std::move(record);
    }
}

const PlayerRecord* PlayerStore::Get(const std::string& nick) const {
    auto it = mRecords.find(ToLower(nick));
    return it == mRecords.end() ? nullptr : &it->second;
}

void PlayerStore::Upload(const std::string& nick, const json& inv, int worldCycle) {
    std::string key = ToLower(nick);
    PlayerRecord& record = mRecords[key];
    record.nick = nick;
    record.inv = inv;
    record.cycle = worldCycle;
    if (record.startCycle != worldCycle) {
        record.start = inv;
        record.startCycle = worldCycle;
    }
    mDirty.insert(key);
}

void PlayerStore::RestoreCycleStart(int oldCycle, int newCycle) {
    for (auto& [key, record] : mRecords) {
        if (record.startCycle == oldCycle && !record.start.is_null()) {
            record.inv = record.start;
            record.cycle = newCycle;
            record.startCycle = newCycle;
            mDirty.insert(key);
        }
    }
}

bool PlayerStore::IsStale(const std::string& nick, int worldCycle) const {
    const PlayerRecord* record = Get(nick);
    return record != nullptr && !record->inv.is_null() && record->cycle < worldCycle;
}

bool PlayerStore::SaveAll(std::string* err) {
    if (mDir.empty()) {
        mDirty.clear();
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(mDir, ec);
    bool ok = true;
    for (auto it = mDirty.begin(); it != mDirty.end();) {
        auto record = mRecords.find(*it);
        if (record == mRecords.end()) {
            it = mDirty.erase(it);
            continue;
        }
        const PlayerRecord& r = record->second;
        json saved = { { "version", 1 }, { "nick", r.nick },       { "cycle", r.cycle }, { "inv", r.inv },
                       { "startCycle", r.startCycle }, { "start", r.start } };
        if (SaveJsonFile(PathOf(*it), saved, err)) {
            it = mDirty.erase(it);
        } else {
            ok = false; // stays dirty: retried on the next save
            ++it;
        }
    }
    return ok;
}

void PlayerStore::Clear() {
    mRecords.clear();
    mDirty.clear();
    if (mDir.empty()) {
        return;
    }
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(mDir, ec)) {
        if (entry.path().extension() == ".json") {
            files.push_back(entry.path());
        }
    }
    for (const auto& file : files) {
        std::filesystem::rename(file, file.string() + ".old", ec);
    }
}

} // namespace coop::server

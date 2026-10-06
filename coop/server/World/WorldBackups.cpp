#include "WorldBackups.h"

#include "JsonFile.h"

#include "common/I18n.h"
#include "common/Text.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace coop::server {

namespace fs = std::filesystem;

namespace {

constexpr size_t kStampSize = 19; // "2026-10-05_14-30-00"

// A backup folder's name: <stamp>[_NN]-<reason>. seq is NN (1 without it): two copies in the same second differ.
struct Parsed {
    std::string name;
    std::string stamp;
    int seq = 1;
    std::string reason;
};

bool ParseName(const std::string& name, Parsed& out) {
    static const char kPattern[] = "dddd-dd-dd_dd-dd-dd"; // d = a digit
    if (name.size() < kStampSize + 2) {
        return false;
    }
    for (size_t i = 0; i < kStampSize; i++) {
        char c = name[i];
        if (kPattern[i] == 'd' ? !(c >= '0' && c <= '9') : c != kPattern[i]) {
            return false;
        }
    }
    size_t at = kStampSize;
    int seq = 1;
    if (name[at] == '_') {
        size_t end = name.find('-', at);
        if (end == std::string::npos || !ParseInt(name.substr(at + 1, end - at - 1), seq) || seq < 2) {
            return false;
        }
        at = end;
    }
    if (name[at] != '-' || at + 1 >= name.size()) {
        return false;
    }
    out.name = name;
    out.stamp = name.substr(0, kStampSize);
    out.seq = seq;
    out.reason = name.substr(at + 1);
    return true;
}

bool Newer(const Parsed& a, const Parsed& b) {
    return a.stamp != b.stamp ? a.stamp > b.stamp : a.seq > b.seq;
}

std::vector<Parsed> Scan(const std::string& dir) {
    std::vector<Parsed> found;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::error_code entryEc;
        Parsed parsed;
        if (entry.is_directory(entryEc) && ParseName(entry.path().filename().string(), parsed)) {
            found.push_back(parsed);
        }
    }
    std::sort(found.begin(), found.end(), Newer); // newest first
    return found;
}

bool ReadBytes(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

std::string TwoDigits(int n) {
    return (n < 10 ? "0" : "") + std::to_string(n);
}

} // namespace

WorldBackups::WorldBackups(std::string dir, int keep) : mDir(std::move(dir)), mKeep(std::max(1, keep)) {
}

void WorldBackups::SetKeep(int keep) {
    mKeep = std::max(1, keep);
}

bool WorldBackups::Make(const std::string& worldPath, const std::string& playersDir, const std::string& reason,
                        std::string* name, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    if (mDir.empty()) {
        return fail(Tr(Msg::BackupOff));
    }
    std::error_code ec;
    if (!fs::exists(worldPath, ec)) {
        return fail(Tr(Msg::FileMissing, { worldPath }));
    }
    // A sequence of its own within the second: the order stays the order they were made in
    std::string stamp = FileStamp();
    std::vector<Parsed> existing = Scan(mDir);
    int seq = 1;
    for (const Parsed& p : existing) {
        if (p.stamp == stamp) {
            seq = std::max(seq, p.seq + 1);
        }
    }
    std::string folderName = stamp + (seq == 1 ? "" : "_" + TwoDigits(seq)) + "-" + reason;
    fs::path folder = fs::path(mDir) / folderName;
    fs::create_directories(folder / "players", ec);
    if (ec) {
        return fail(Tr(Msg::FileWriteFail, { folder.string() }));
    }
    fs::copy_file(worldPath, folder / "world.json", fs::copy_options::overwrite_existing, ec);
    if (ec) {
        fs::remove_all(folder, ec);
        return fail(Tr(Msg::FileWriteFail, { (folder / "world.json").string() }));
    }
    for (const auto& entry : fs::directory_iterator(playersDir, ec)) {
        std::error_code fileEc;
        if (entry.is_regular_file(fileEc) && entry.path().extension() == ".json") {
            fs::copy_file(entry.path(), folder / "players" / entry.path().filename(),
                          fs::copy_options::overwrite_existing, fileEc);
        }
    }
    Prune();
    if (name != nullptr) {
        *name = folderName;
    }
    return true;
}

void WorldBackups::Prune() {
    std::vector<Parsed> automatic;
    for (const Parsed& p : Scan(mDir)) {
        if (p.reason != "manual") {
            automatic.push_back(p);
        }
    }
    std::error_code ec;
    for (size_t i = (size_t)mKeep; i < automatic.size(); i++) { // newest first: everything after `keep` goes
        fs::remove_all(fs::path(mDir) / automatic[i].name, ec);
    }
}

std::vector<BackupInfo> WorldBackups::List() const {
    std::vector<BackupInfo> list;
    if (mDir.empty()) {
        return list;
    }
    for (const Parsed& p : Scan(mDir)) {
        list.push_back({ p.name, p.reason });
    }
    return list;
}

std::string WorldBackups::Newest() const {
    std::vector<BackupInfo> list = List();
    return list.empty() ? "" : list.front().name;
}

std::string WorldBackups::Resolve(const std::string& nameOrLast) const {
    std::string lower = ToLower(nameOrLast);
    if (lower == "ultima" || lower == "última" || lower == "last" || lower == "latest") {
        return Newest();
    }
    std::string match;
    int matches = 0;
    for (const BackupInfo& b : List()) {
        if (b.name == nameOrLast) {
            return b.name;
        }
        if (!nameOrLast.empty() && b.name.rfind(nameOrLast, 0) == 0) {
            match = b.name;
            matches++;
        }
    }
    return matches == 1 ? match : "";
}

std::string WorldBackups::WorldFile(const std::string& name) const {
    return (fs::path(mDir) / name / "world.json").string();
}

std::string WorldBackups::PlayersDir(const std::string& name) const {
    return (fs::path(mDir) / name / "players").string();
}

bool WorldBackups::SameAsNewest(const std::string& worldPath) const {
    std::string newest = Newest();
    std::string now;
    std::string saved;
    return !newest.empty() && ReadBytes(worldPath, now) && ReadBytes(WorldFile(newest), saved) && now == saved;
}

} // namespace coop::server

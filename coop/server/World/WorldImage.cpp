#include "WorldImage.h"

#include "JsonFile.h"

#include "common/Clock.h"

#include <algorithm>

namespace coop::server {

json WorldFileJson(const WorldStore& store, uint32_t abs, bool inverted, bool frozen) {
    json saved = store.ToJson();
    saved["clock"] = json{ { "abs", abs }, { "inv", inverted }, { "frozen", frozen } };
    return saved;
}

bool ParseWorldFile(const json& saved, WorldImage& out, std::vector<std::string>* warnings, std::string* err) {
    WorldStore store;
    if (!store.FromJson(saved, err, warnings)) {
        return false;
    }
    json clockJson = saved.value("clock", json::object());
    out.fields = store.Fields();
    out.start = store.Start();
    out.cycle = store.Cycle();
    out.clockAbs = (uint32_t)std::clamp<int64_t>(GetInt(clockJson, "abs", 0), 0, clock::kMoonAbs);
    out.inverted = GetBool(clockJson, "inv");
    out.frozen = GetBool(clockJson, "frozen");
    out.players.clear();
    return true;
}

bool LoadWorldImage(const std::string& worldPath, const std::string& playersDir, WorldImage& out,
                    std::vector<std::string>* warnings, std::string* err) {
    json saved;
    if (!LoadJsonFile(worldPath, saved, err) || !ParseWorldFile(saved, out, warnings, err)) {
        return false;
    }
    std::string playerWarnings;
    PlayerStore::LoadDir(playersDir, out.players, &playerWarnings);
    if (!playerWarnings.empty() && warnings != nullptr) {
        warnings->push_back(playerWarnings);
    }
    return true;
}

bool SaveWorldImage(const WorldImage& image, const std::string& worldPath, const std::string& playersDir,
                    std::string* err) {
    WorldStore store;
    store.Replace(image.fields, image.start, image.cycle);
    if (!SaveJsonFile(worldPath, WorldFileJson(store, image.clockAbs, image.inverted, image.frozen), err)) {
        return false;
    }
    PlayerStore players(playersDir);
    players.ReplaceAll(image.players);
    return players.SaveAll(err);
}

WorldImage ImageFromImport(const save::Imported& imported, const std::string& nick) {
    WorldImage image;
    image.fields = imported.fields;
    image.start = imported.start;
    image.cycle = 1;
    image.clockAbs = imported.clockAbs;
    image.inverted = imported.inverted;
    PlayerRecord record;
    record.nick = nick;
    record.inv = imported.inv;
    record.cycle = 1;
    record.start = imported.invStart;
    record.startCycle = 1;
    image.players.push_back(std::move(record));
    return image;
}

} // namespace coop::server

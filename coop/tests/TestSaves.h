#pragma once
// A 2 Ship save as its JSON file holds it (saves/fileN.json), for the tests of the save converter and of /importar.
#include <nlohmann/json.hpp>

#include <vector>

namespace coop_test {

inline nlohmann::json Filled(size_t count, int value) {
    return nlohmann::json(std::vector<int>(count, value));
}

inline nlohmann::json Grid(size_t rows, size_t cols, int value) {
    nlohmann::json out = nlohmann::json::array();
    for (size_t r = 0; r < rows; r++) {
        out.push_back(Filled(cols, value));
    }
    return out;
}

// A save as 2 Ship writes a brand new file (version 8): nothing in the inventory, no Tatl, Day 1 before dawn.
inline nlohmann::json MakeSave() {
    using json = nlohmann::json;
    json scene = { { "chest", 0 },       { "switch0", 0 }, { "switch1", 0 }, { "clearedRoom", 0 },
                   { "collectible", 0 }, { "unk_14", 0 },  { "rooms", 0 } };
    json player = { { "newf", { 90, 69, 76, 68, 65, 51 } },
                    { "threeDayResetCount", 0 },
                    { "playerName", { 21, 44, 49, 46, 62, 62, 62, 62 } }, // "Link"
                    { "healthCapacity", 48 },
                    { "health", 48 },
                    { "magicLevel", 0 },
                    { "magic", 48 },
                    { "rupees", 0 },
                    { "swordHealth", 0 },
                    { "tatlTimer", 0 },
                    { "isMagicAcquired", 0 },
                    { "isDoubleMagicAcquired", 0 },
                    { "doubleDefense", 0 },
                    { "unk_1F", 0 },
                    { "unk_20", 255 },
                    { "owlActivationFlags", 0 },
                    { "unk_24", 255 },
                    { "savedSceneId", 8 } };
    json inventory = { { "items", Filled(48, 255) },         { "ammo", Filled(24, 0) },
                       { "upgrades", 0x120000 },             { "questItems", 0 },
                       { "dungeonItems", Filled(10, 0) },    { "dungeonKeys", Filled(9, -1) },
                       { "defenseHearts", 0 },               { "strayFairies", Filled(10, 0) },
                       { "dekuPlaygroundPlayerName", Grid(3, 8, 0) } };
    json info = { { "playerData", player },
                  { "equips", { { "buttonItems", Grid(4, 4, 255) }, { "cButtonSlots", Grid(4, 4, 255) },
                                { "equipment", 0 } } },
                  { "inventory", inventory },
                  { "permanentSceneFlags", json(std::vector<json>(120, scene)) },
                  { "unk_DF4", Filled(84, 0) },
                  { "dekuPlaygroundHighScores", Filled(3, 0) },
                  { "pictoFlags0", 0 },
                  { "pictoFlags1", 0 },
                  { "unk_E5C", 0 },
                  { "unk_E60", 0 },
                  { "unk_E64", Filled(7, 0) },
                  { "scenesVisible", Filled(7, 0) },
                  { "skullTokenCount", 0 },
                  { "unk_EA0", 0 },
                  { "unk_EA4", 0 },
                  { "unk_EA8", Filled(2, 0) },
                  { "stolenItems", 0 },
                  { "unk_EB4", 0 },
                  { "highScores", Filled(7, 0) },
                  { "weekEventReg", Filled(100, 0) },
                  { "regionsVisited", 0 },
                  { "worldMapCloudVisibility", 0 },
                  { "unk_F40", 0 },
                  { "scarecrowSpawnSongSet", 0 },
                  { "scarecrowSpawnSong", Filled(128, 0) },
                  { "bombersCaughtNum", 0 },
                  { "bombersCaughtOrder", Filled(5, 0) },
                  { "lotteryCodes", Grid(3, 3, 0) },
                  { "spiderHouseMaskOrder", Filled(6, 0) },
                  { "bomberCode", Filled(5, 0) },
                  { "horseData", { { "sceneId", 0 }, { "pos", { { "x", 0 }, { "y", 0 }, { "z", 0 } } }, { "yaw", 0 } } },
                  { "checksum", 0 } };
    json ship = { { "dpadEquips", { { "dpadItems", Grid(4, 4, 255) }, { "dpadSlots", Grid(4, 4, 255) } } },
                  { "pauseSaveEntrance", -1 },
                  { "saveType", 0 },
                  { "fileCreatedAt", 0 },
                  { "fileCompletedAt", 0 },
                  { "filePlaytime", 0 },
                  { "respawn", json::array() },
                  { "commitHash", Filled(8, 0) },
                  { "persistentBunnyHood", 0 } };
    return { { "entrance", 7168 }, { "equippedMask", 0 },   { "isFirstCycle", 0 },  { "unk_06", 0 },
             { "linkAge", 0 },      { "cutsceneIndex", 0 }, { "time", 16383 },      { "owlSaveLocation", 255 },
             { "isNight", 0 },      { "timeSpeedOffset", 0 }, { "day", 0 },         { "eventDayCount", 0 },
             { "playerForm", 4 },   { "snowheadCleared", 0 }, { "hasTatl", 0 },     { "isOwlSave", 0 },
             { "saveInfo", info },  { "shipSaveInfo", ship } };
}

inline nlohmann::json MakeFile(const nlohmann::json& cycle, const nlohmann::json* owl = nullptr) {
    using json = nlohmann::json;
    json file = { { "type", "2S2H_SAVE" }, { "version", 8 }, { "newCycleSave", { { "save", cycle } } } };
    if (owl != nullptr) {
        file["owlSave"] = { { "save", *owl }, { "eventInf", Filled(8, 0) } };
    }
    return file;
}

} // namespace coop_test

// SaveImport: base-game saves (2 Ship's saves/fileN.json) into the co-op's world and player data.
#include "TestMain.h"
#include "TestSaves.h"

#include "common/Clock.h"
#include "common/Events.h"
#include "common/Hex.h"
#include "common/SaveImport.h"
#include "common/WorldFields.h"
#include "server/World/WorldStore.h"

#include <string>
#include <vector>

using namespace coop;
using json = nlohmann::json;
using namespace coop_test;

namespace {

std::vector<uint8_t> F(const save::Imported& imp, const char* name) {
    return imp.fields[(size_t)world::FindField(name)];
}

std::vector<uint8_t> P(const json& inv, const char* name) {
    std::vector<uint8_t> bytes;
    CHECK(FromHex(inv["fields"][name].get<std::string>(), bytes));
    return bytes;
}

std::vector<uint8_t> B(std::initializer_list<int> values) {
    std::vector<uint8_t> out;
    for (int v : values) {
        out.push_back((uint8_t)v);
    }
    return out;
}

save::ImportOptions Raw() { // exact mapping: no co-op minimum
    save::ImportOptions opts;
    opts.baseline = false;
    return opts;
}

bool Import(const json& file, const save::ImportOptions& opts, save::Imported& out, std::string* err = nullptr) {
    std::string why;
    bool ok = save::ImportSave(file, opts, out, &why);
    if (err != nullptr) {
        *err = why;
    }
    return ok;
}

} // namespace

TEST_CASE(ImportCycleSaveMapsEveryField) {
    json s = MakeSave();
    json& info = s["saveInfo"];
    json& pd = info["playerData"];
    json& inv = info["inventory"];
    pd["rupees"] = 300;
    pd["health"] = 0x40;
    pd["magic"] = 0x30;
    pd["swordHealth"] = 77;
    pd["healthCapacity"] = 0x50;
    pd["threeDayResetCount"] = 3;
    pd["owlActivationFlags"] = 0x0201;
    pd["isMagicAcquired"] = 1;
    pd["doubleDefense"] = 1;
    inv["items"][0] = 0x00;
    inv["items"][1] = 0x01;
    inv["items"][18] = 0x12;
    inv["items"][19] = 0x16;
    inv["items"][24] = 0x3E;
    inv["ammo"][1] = 25;
    inv["upgrades"] = 0x1009;
    inv["questItems"] = (2u << 28) | (1u << 25) | (1u << 12) | 1u; // 2 loose pieces, a photo, Song of Time, Odolwa
    inv["dungeonItems"][0] = 5;
    inv["dungeonKeys"][0] = 2;
    inv["strayFairies"][0] = 15;
    inv["dekuPlaygroundPlayerName"][1][0] = 10;
    info["equips"]["equipment"] = 0x21;
    info["equips"]["buttonItems"][0][0] = 0x4D;
    info["equips"]["cButtonSlots"][0][1] = 1;
    info["permanentSceneFlags"][5] = { { "chest", 0x11 },       { "switch0", 0x22 }, { "switch1", 0x33 },
                                       { "clearedRoom", 0x44 }, { "collectible", 0x55 }, { "unk_14", 0x66 },
                                       { "rooms", 0x77 } };
    info["weekEventReg"][92] = 0xFF;
    info["weekEventReg"][10] = 0x5A;
    info["scenesVisible"][0] = 0xAABBCCDDu;
    info["regionsVisited"] = 3;
    info["worldMapCloudVisibility"] = 0x10;
    info["unk_EA0"] = 7;
    info["unk_EA8"] = { 1, 2 };
    info["skullTokenCount"] = 0x00050003;
    info["stolenItems"] = 0x12000000u; // Takkuri has a bottle
    info["lotteryCodes"] = { { 1, 2, 3 }, { 4, 5, 6 }, { 7, 8, 9 } };
    info["spiderHouseMaskOrder"] = { 0, 1, 2, 3, 4, 5 };
    info["bomberCode"] = { 1, 2, 3, 4, 5 };
    info["highScores"][0] = 5000;
    info["dekuPlaygroundHighScores"][0] = 1000;
    info["pictoFlags0"] = 0x10;
    info["pictoFlags1"] = 0x20;
    info["unk_F40"] = 1;
    info["scarecrowSpawnSongSet"] = 1;
    info["scarecrowSpawnSong"][0] = 9;
    info["bombersCaughtNum"] = 2;
    info["bombersCaughtOrder"] = { 1, 2, 0, 0, 0 };
    info["horseData"] = { { "sceneId", 0x35 }, { "pos", { { "x", 1 }, { "y", -2 }, { "z", 3 } } }, { "yaw", -100 } };
    s["snowheadCleared"] = 1;
    s["hasTatl"] = 1;
    s["playerForm"] = 3; // Deku: a cycle-start save loads as human Link
    s["shipSaveInfo"]["dpadEquips"]["dpadItems"][0][0] = 6;

    save::Imported imp;
    CHECK(Import(MakeFile(s), Raw(), imp));
    std::vector<uint8_t> items = F(imp, "items");
    CHECK_EQ(items[0], 0x00);
    CHECK_EQ(items[1], 0x01);
    CHECK_EQ(items[2], 0xFF);
    CHECK_EQ(items.size(), (size_t)18);
    CHECK_EQ(F(imp, "masks")[0], 0x3E);
    CHECK(F(imp, "quest") == B({ 0x01, 0x10, 0x00, 0x00 })); // no heart pieces, no photo
    CHECK(F(imp, "heartQuarters") == B({ 22, 0 }));          // 5 containers + 2 pieces
    CHECK_EQ(F(imp, "bottles")[0], 3);                       // two in the slots + Takkuri's
    CHECK_EQ(F(imp, "weekEventReg")[92], 0x78);              // the local bits stay out
    CHECK_EQ(F(imp, "weekEventReg")[10], 0x5A);
    std::vector<uint8_t> flags = F(imp, "sceneFlags");
    CHECK(std::vector<uint8_t>(flags.begin() + 100, flags.begin() + 120) ==
          B({ 0x11, 0, 0, 0, 0x22, 0, 0, 0, 0x33, 0, 0, 0, 0x44, 0, 0, 0, 0x55, 0, 0, 0 }));
    CHECK_EQ(F(imp, "sceneRooms")[20], 0x77);
    CHECK_EQ(F(imp, "sceneExtra")[20], 0x66);
    CHECK(F(imp, "owls") == B({ 0x01, 0x02 }));
    std::vector<uint8_t> maps = F(imp, "mapsVisible");
    CHECK(std::vector<uint8_t>(maps.begin(), maps.begin() + 4) == B({ 0xDD, 0xCC, 0xBB, 0xAA }));
    CHECK(F(imp, "regions") == B({ 3, 0, 0, 0 }));
    CHECK(F(imp, "clouds") == B({ 0x10, 0, 0, 0 }));
    CHECK(F(imp, "gossipHearts") == B({ 7, 0, 0, 0 }));
    CHECK(F(imp, "blueWarps") == B({ 1, 0, 0, 0, 2, 0, 0, 0 }));
    CHECK(F(imp, "upgrades") == B({ 0x09, 0x10, 0, 0 }));
    CHECK_EQ(F(imp, "dungeonItems")[0], 5);
    CHECK(F(imp, "equipment") == B({ 0x21, 0x00 }));
    CHECK(F(imp, "magicFlags") == B({ 1, 0 }));
    CHECK(F(imp, "defense") == B({ 1 }));
    CHECK(F(imp, "progress") == B({ 0, 1, 1 }));
    CHECK(F(imp, "resets") == B({ 3, 0 }));
    CHECK(F(imp, "codes") == B({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 1, 2, 3, 4, 5 }));
    CHECK_EQ(F(imp, "keys")[0], 2);
    CHECK_EQ(F(imp, "keys")[1], 0xFF);
    CHECK_EQ(F(imp, "fairies")[0], 15);
    CHECK(F(imp, "skulls") == B({ 3, 0, 5, 0 }));

    const json& own = imp.inv;
    CHECK_EQ(own["v"].get<int>(), 1);
    CHECK(P(own, "rupees") == B({ 0x2C, 0x01 }));
    CHECK(P(own, "health") == B({ 0x40, 0x00 }));
    CHECK(P(own, "magic") == B({ 0x30 }));
    CHECK(P(own, "swordHealth") == B({ 77, 0 }));
    CHECK_EQ(P(own, "ammo")[1], 25);
    CHECK(P(own, "bottleItems") == B({ 0x12, 0x16, 0xFF, 0xFF, 0xFF, 0xFF }));
    CHECK_EQ(P(own, "buttonItems")[0], 0x4D);
    CHECK_EQ(P(own, "cButtonSlots")[1], 1);
    CHECK_EQ(P(own, "dpad")[0], 6);
    CHECK_EQ(P(own, "dpad")[16], 0xFF);
    CHECK(P(own, "form") == B({ 4 }));
    CHECK(P(own, "mask") == B({ 0 }));
    std::vector<uint8_t> scores = P(own, "highScores");
    CHECK(std::vector<uint8_t>(scores.begin(), scores.begin() + 4) == B({ 0x88, 0x13, 0, 0 })); // the bank
    std::vector<uint8_t> deku = P(own, "dekuScores");
    CHECK(std::vector<uint8_t>(deku.begin(), deku.begin() + 4) == B({ 0xE8, 0x03, 0, 0 }));
    CHECK_EQ(P(own, "dekuNames")[8], 10);
    CHECK(P(own, "picto") == B({ 0x10, 0, 0, 0, 0x20, 0, 0, 0 }));
    CHECK(P(own, "stolen") == B({ 0, 0, 0, 0x12 }));
    std::vector<uint8_t> scarecrow = P(own, "scarecrow");
    CHECK_EQ(scarecrow.size(), (size_t)130);
    CHECK(std::vector<uint8_t>(scarecrow.begin(), scarecrow.begin() + 3) == B({ 1, 1, 9 }));
    CHECK(P(own, "horse") == B({ 0x35, 0, 1, 0, 0xFE, 0xFF, 3, 0, 0x9C, 0xFF }));
    CHECK(P(own, "bombers") == B({ 2, 1, 2, 0, 0, 0 }));
    CHECK(!own.contains("entrance"));

    CHECK(!imp.fromOwl);
    CHECK_EQ(imp.clockAbs, 0u);
    CHECK(!imp.inverted);
    CHECK(imp.start == imp.fields);
    CHECK_EQ(imp.invStart, imp.inv);
    CHECK_EQ(imp.version, 8);
    CHECK_EQ(imp.playerName, std::string("Link"));
    CHECK_EQ(imp.baseline, 0u);
}

TEST_CASE(ImportOwlSaveUsesItsTimeAndSpawn) {
    json cycle = MakeSave();
    json owl = cycle;
    owl["day"] = 2;
    owl["time"] = 0xC000; // 18:00
    owl["timeSpeedOffset"] = -2;
    owl["owlSaveLocation"] = 0; // Great Bay Coast
    owl["isOwlSave"] = 1;
    owl["playerForm"] = 2; // Zora: an owl save keeps the form
    owl["saveInfo"]["inventory"]["items"][1] = 0x01; // the bow, found this cycle
    owl["saveInfo"]["playerData"]["rupees"] = 50;
    json file = MakeFile(cycle, &owl);

    save::Imported imp;
    CHECK(Import(file, Raw(), imp));
    CHECK(imp.fromOwl);
    CHECK_EQ(imp.clockAbs, clock::AbsOf(2, 0xC000));
    CHECK(imp.inverted);
    CHECK_EQ(imp.inv["entrance"].get<int>(), (int)save::Entrance(0x34, 11));
    CHECK_EQ(F(imp, "items")[1], 0x01);
    CHECK_EQ(imp.start[(size_t)world::FindField("items")][1], 0xFF); // the moon goes back to the cycle's start
    CHECK(P(imp.inv, "form") == B({ 2 }));
    CHECK(P(imp.invStart, "form") == B({ 4 }));
    CHECK(P(imp.inv, "rupees") == B({ 50, 0 }));
    CHECK(P(imp.invStart, "rupees") == B({ 0, 0 }));
    CHECK(!imp.invStart.contains("entrance"));

    save::ImportOptions cycleOnly = Raw();
    cycleOnly.source = save::ImportSource::Cycle;
    save::Imported start;
    CHECK(Import(file, cycleOnly, start));
    CHECK(!start.fromOwl);
    CHECK_EQ(start.clockAbs, 0u);
    CHECK_EQ(F(start, "items")[1], 0xFF);
}

TEST_CASE(ImportOwlSpawnVariants) {
    json cycle = MakeSave();
    json owl = cycle;
    owl["day"] = 1;
    owl["time"] = 0x8000;
    owl["shipSaveInfo"]["pauseSaveEntrance"] = 0x1234; // 2 Ship's pause save wins over the owl statue
    owl["owlSaveLocation"] = 0;
    save::Imported imp;
    CHECK(Import(MakeFile(cycle, &owl), Raw(), imp));
    CHECK_EQ(imp.inv["entrance"].get<int>(), 0x1234);

    owl["shipSaveInfo"]["pauseSaveEntrance"] = -1;
    owl["owlSaveLocation"] = 7;                   // Southern Swamp...
    owl["saveInfo"]["weekEventReg"][20] = 0x02;   // ...with Woodfall cleared
    CHECK(Import(MakeFile(cycle, &owl), Raw(), imp));
    CHECK_EQ(imp.inv["entrance"].get<int>(), (int)save::kEntranceSwampClearedOwl);

    owl["owlSaveLocation"] = 0xFF; // no owl: the co-op's usual start
    CHECK(Import(MakeFile(cycle, &owl), Raw(), imp));
    CHECK(!imp.inv.contains("entrance"));

    owl["day"] = 0; // before the first dawn
    CHECK(Import(MakeFile(cycle, &owl), Raw(), imp));
    CHECK_EQ(imp.clockAbs, 0u);
}

TEST_CASE(ImportAddsTheCoopMinimum) {
    save::Imported imp;
    CHECK(Import(MakeFile(MakeSave()), save::ImportOptions(), imp));
    CHECK((imp.baseline & save::BaselineOcarina) != 0);
    CHECK((imp.baseline & save::BaselineTatl) != 0);
    CHECK_EQ(F(imp, "items")[0], save::kItemOcarinaOfTime);
    CHECK_EQ(imp.start[(size_t)world::FindField("items")][0], save::kItemOcarinaOfTime);
    // On the buttons a new co-op player gets them on
    CHECK_EQ(P(imp.inv, "buttonItems")[save::kEquipSlotCDown], save::kItemOcarinaOfTime);
    CHECK_EQ(P(imp.inv, "cButtonSlots")[save::kEquipSlotCDown], save::kSlotOcarina);
    CHECK_EQ(P(imp.inv, "buttonItems")[save::kEquipSlotCLeft], save::kItemMaskDeku);
    CHECK_EQ(P(imp.inv, "cButtonSlots")[save::kEquipSlotCLeft], save::kSlotMaskDeku);
    bool said = false;
    for (const std::string& line : save::DescribeImport(imp)) {
        said = said || line.find("Ocarina del Tiempo") != std::string::npos;
    }
    CHECK(said);
}

TEST_CASE(ImportRefusesWhatIsNotASave) {
    save::Imported imp;
    std::string err;
    CHECK(!Import(json::array(), save::ImportOptions(), imp, &err));
    CHECK(!err.empty());
    CHECK(!Import(json::object(), save::ImportOptions(), imp, &err));
    CHECK(!Import({ { "type", "2S2H_SAVE" }, { "version", 8 } }, save::ImportOptions(), imp, &err));
    CHECK(!Import({ { "type", "OTHER" }, { "newCycleSave", { { "save", MakeSave() } } } }, save::ImportOptions(), imp,
                  &err));

    json rando = MakeSave();
    rando["shipSaveInfo"]["saveType"] = 1;
    CHECK(!Import(MakeFile(rando), save::ImportOptions(), imp, &err));
    CHECK(err.find("randomizer") != std::string::npos);

    json erased = MakeSave();
    erased["saveInfo"]["playerData"]["newf"] = Filled(6, 0);
    CHECK(!Import(MakeFile(erased), save::ImportOptions(), imp, &err));

    save::ImportOptions owlOnly;
    owlOnly.source = save::ImportSource::Owl;
    CHECK(!Import(MakeFile(MakeSave()), owlOnly, imp, &err));

    json broken = MakeSave();
    broken["saveInfo"]["inventory"]["items"] = "x";
    CHECK(!Import(MakeFile(broken), save::ImportOptions(), imp, &err));
    CHECK(err.find("items") != std::string::npos);
}

TEST_CASE(ImportReadsOldVersions) {
    // Version 7 (the user's own file): no persistentBunnyHood; older still: no D-pad at all
    json old = MakeSave();
    old["shipSaveInfo"].erase("persistentBunnyHood");
    old["shipSaveInfo"].erase("dpadEquips");
    json file = MakeFile(old);
    file["version"] = 7;
    save::Imported imp;
    CHECK(Import(file, Raw(), imp));
    CHECK(P(imp.inv, "dpad") == std::vector<uint8_t>(32, 0xFF));
    // Before version 4 the file held one Save: {"save": Save} (versions 1-3) or the Save itself (version 0)
    CHECK(Import({ { "version", 2 }, { "save", MakeSave() } }, Raw(), imp));
    CHECK(!imp.fromOwl);
    CHECK(Import(MakeSave(), Raw(), imp));
    CHECK_EQ(imp.playerName, std::string("Link"));
}

TEST_CASE(ImportedWorldFitsTheServer) {
    json s = MakeSave();
    s["saveInfo"]["scarecrowSpawnSong"] = Filled(128, 0xFF);
    save::Imported imp;
    CHECK(Import(MakeFile(s), save::ImportOptions(), imp));
    server::FieldSet parsed;
    std::string err;
    CHECK(server::WorldStore::ParseFields(save::FieldsToJson(imp.fields), parsed, &err));
    CHECK(SerializeEvent(imp.inv).size() < kMaxInventoryBytes);
    CHECK_EQ(save::DecodePlayerName(json({ 1, 0x0A, 0x24, 62, 0x40 })), std::string("1Aa"));
}

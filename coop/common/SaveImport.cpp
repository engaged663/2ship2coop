#include "SaveImport.h"

#include "Clock.h"
#include "Hex.h"
#include "I18n.h"
#include "Text.h"
#include "WorldFields.h"

#include <algorithm>

namespace coop::save {

namespace {

constexpr int kKnownSaveVersion = 8;                            // 2 Ship's CURRENT_SAVE_VERSION when this was written
constexpr int64_t kSaveTypeRando = 1;                           // SAVETYPE_RANDO
constexpr int kSongBits[] = { 6, 7, 8, 9, 10, 12, 13, 14, 15, 16 }; // QUEST_SONG_SONATA..STORMS without Saria's

// What is missing or malformed in the file, as a path ("save.saveInfo.inventory.items").
struct ImportError {
    std::string path;
};

const json& Get(const json& obj, const char* key, const std::string& where) {
    if (!obj.is_object()) {
        throw ImportError{ where };
    }
    auto it = obj.find(key);
    if (it == obj.end()) {
        throw ImportError{ where + "." + key };
    }
    return *it;
}

int64_t Num(const json& value, const std::string& where) {
    if (value.is_boolean()) {
        return value.get<bool>() ? 1 : 0;
    }
    if (value.is_number_integer()) {
        return value.get<int64_t>();
    }
    if (value.is_number_float()) {
        return (int64_t)value.get<double>();
    }
    throw ImportError{ where };
}

int64_t NumAt(const json& obj, const char* key, const std::string& where) {
    return Num(Get(obj, key, where), where + "." + key);
}

std::vector<int64_t> Nums(const json& value, size_t count, const std::string& where) {
    if (!value.is_array() || value.size() != count) {
        throw ImportError{ where };
    }
    std::vector<int64_t> out;
    for (const json& v : value) {
        out.push_back(Num(v, where));
    }
    return out;
}

std::vector<int64_t> NumsAt(const json& obj, const char* key, size_t count, const std::string& where) {
    return Nums(Get(obj, key, where), count, where + "." + key);
}

// rows arrays of cols numbers, row after row (the game's two-dimensional arrays).
std::vector<int64_t> GridAt(const json& obj, const char* key, size_t rows, size_t cols, const std::string& where) {
    std::string path = where + "." + key;
    const json& value = Get(obj, key, where);
    if (!value.is_array() || value.size() != rows) {
        throw ImportError{ path };
    }
    std::vector<int64_t> out;
    for (const json& row : value) {
        std::vector<int64_t> numbers = Nums(row, cols, path);
        out.insert(out.end(), numbers.begin(), numbers.end());
    }
    return out;
}

// Little-endian bytes, as the game keeps its values in memory (signed ones wrap: -1 is 0xFF).
struct Out {
    std::vector<uint8_t> bytes;
    void U8(int64_t v) {
        bytes.push_back((uint8_t)(v & 0xFF));
    }
    void U16(int64_t v) {
        U8(v);
        U8(v >> 8);
    }
    void U32(int64_t v) {
        U16(v);
        U16(v >> 16);
    }
    void U8s(const std::vector<int64_t>& values) {
        for (int64_t v : values) {
            U8(v);
        }
    }
    void U32s(const std::vector<int64_t>& values) {
        for (int64_t v : values) {
            U32(v);
        }
    }
};

// The objects of a Save the converter reads, with their paths for the error messages.
struct SaveParts {
    const json& save;
    const json& info;
    const json& player;
    const json& inventory;
    const json& equips;
    static constexpr const char* S = "save";
    static constexpr const char* I = "save.saveInfo";
    static constexpr const char* P = "save.saveInfo.playerData";
    static constexpr const char* V = "save.saveInfo.inventory";
    static constexpr const char* E = "save.saveInfo.equips";

    explicit SaveParts(const json& s)
        : save(s), info(Get(s, "saveInfo", S)), player(Get(info, "playerData", I)), inventory(Get(info, "inventory", I)),
          equips(Get(info, "equips", I)) {
    }
};

FieldBytes WorldOf(const json& saveJson) {
    SaveParts p(saveJson);
    FieldBytes f = EmptyFields();
    auto put = [&f](const char* name, const Out& o) {
        int i = world::FindField(name);
        if (i < 0 || o.bytes.size() != world::kFields[i].size) {
            throw ImportError{ std::string("schema.") + name }; // this file and WorldFields.h disagree
        }
        f[(size_t)i] = o.bytes;
    };
    {
        Out o;
        std::vector<int64_t> week = NumsAt(p.info, "weekEventReg", 100, p.I);
        for (int i = 0; i < 100; i++) {
            o.U8(week[(size_t)i] & ~LocalWeekEventMask(i));
        }
        put("weekEventReg", o);
    }
    {
        // The scene flags of the cycle travel in permanentSceneFlags: Sram_OpenSave copies them back when loading
        std::string where = std::string(p.I) + ".permanentSceneFlags";
        const json& scenes = Get(p.info, "permanentSceneFlags", p.I);
        if (!scenes.is_array() || scenes.size() != (size_t)kSceneCount) {
            throw ImportError{ where };
        }
        Out flags;
        Out rooms;
        Out extra;
        for (int n = 0; n < kSceneCount; n++) {
            const json& scene = scenes[(size_t)n];
            std::string at = where + "[" + std::to_string(n) + "]";
            for (const char* key : { "chest", "switch0", "switch1", "clearedRoom", "collectible" }) {
                flags.U32(NumAt(scene, key, at));
            }
            rooms.U32(NumAt(scene, "rooms", at));
            extra.U32(NumAt(scene, "unk_14", at));
        }
        put("sceneFlags", flags);
        put("sceneRooms", rooms);
        put("sceneExtra", extra);
    }
    Out owls;
    owls.U16(NumAt(p.player, "owlActivationFlags", p.P));
    put("owls", owls);
    Out maps;
    maps.U32s(NumsAt(p.info, "scenesVisible", 7, p.I));
    put("mapsVisible", maps);
    Out regions;
    regions.U32(NumAt(p.info, "regionsVisited", p.I));
    put("regions", regions);
    Out clouds;
    clouds.U32(NumAt(p.info, "worldMapCloudVisibility", p.I));
    put("clouds", clouds);
    Out gossip;
    gossip.U32(NumAt(p.info, "unk_EA0", p.I));
    put("gossipHearts", gossip);
    Out warps;
    warps.U32s(NumsAt(p.info, "unk_EA8", 2, p.I));
    put("blueWarps", warps);
    Out upgrades;
    upgrades.U32(NumAt(p.inventory, "upgrades", p.V));
    put("upgrades", upgrades);
    int64_t questItems = NumAt(p.inventory, "questItems", p.V);
    Out quest;
    quest.U32(questItems & ~(int64_t)kLocalQuestBits);
    put("quest", quest);
    Out dungeon;
    dungeon.U8s(NumsAt(p.inventory, "dungeonItems", 10, p.V));
    put("dungeonItems", dungeon);
    std::vector<int64_t> items = NumsAt(p.inventory, "items", 48, p.V);
    Out itemSlots;
    for (int s = 0; s < kItemFieldSlots; s++) {
        itemSlots.U8(items[(size_t)s]);
    }
    put("items", itemSlots);
    Out maskSlots;
    for (int s = 0; s < kMaskSlots; s++) {
        maskSlots.U8(items[(size_t)(kSlotMaskFirst + s)]);
    }
    put("masks", maskSlots);
    Out equipment;
    equipment.U16(NumAt(p.equips, "equipment", p.E));
    put("equipment", equipment);
    Out magic;
    magic.U8(NumAt(p.player, "isMagicAcquired", p.P));
    magic.U8(NumAt(p.player, "isDoubleMagicAcquired", p.P));
    put("magicFlags", magic);
    Out defense;
    defense.U8(NumAt(p.player, "doubleDefense", p.P));
    put("defense", defense);
    Out progress;
    progress.U8(NumAt(p.save, "isFirstCycle", p.S));
    progress.U8(NumAt(p.save, "snowheadCleared", p.S));
    progress.U8(NumAt(p.save, "hasTatl", p.S));
    put("progress", progress);
    Out resets;
    resets.U16(NumAt(p.player, "threeDayResetCount", p.P));
    put("resets", resets);
    Out codes;
    codes.U8s(GridAt(p.info, "lotteryCodes", 3, 3, p.I));
    codes.U8s(NumsAt(p.info, "spiderHouseMaskOrder", 6, p.I));
    codes.U8s(NumsAt(p.info, "bomberCode", 5, p.I));
    put("codes", codes);
    Out hearts; // containers * 4 + loose pieces, as FieldTable's ReadHeartQuarters
    hearts.U16(NumAt(p.player, "healthCapacity", p.P) / 0x10 * 4 + ((questItems >> kQuestHeartPieceShift) & 0xF));
    put("heartQuarters", hearts);
    int64_t stolen = NumAt(p.info, "stolenItems", p.I);
    Out bottles; // the bottles owned: the slots in use and the ones Takkuri stole (STOLEN_ITEM_1/2)
    int count = 0;
    for (int s = 0; s < kBottleSlots; s++) {
        count += (uint8_t)items[(size_t)(kSlotBottle1 + s)] != kItemNone;
    }
    count += ((stolen >> 24) & 0xFF) == kItemBottle;
    count += ((stolen >> 16) & 0xFF) == kItemBottle;
    bottles.U8(count);
    put("bottles", bottles);
    Out keys;
    keys.U8s(NumsAt(p.inventory, "dungeonKeys", 9, p.V));
    put("keys", keys);
    Out fairies;
    fairies.U8s(NumsAt(p.inventory, "strayFairies", 10, p.V));
    put("fairies", fairies);
    Out skulls;
    skulls.U32(NumAt(p.info, "skullTokenCount", p.I));
    put("skulls", skulls);
    return f;
}

json PlayerOf(const json& saveJson, bool human) {
    SaveParts p(saveJson);
    json fields = json::object();
    auto put = [&fields](const char* name, const Out& o) {
        int i = FindPlayerField(name);
        if (i < 0 || o.bytes.size() != kPlayerFields[i].size) {
            throw ImportError{ std::string("schema.") + name }; // this file and SaveLayout.h disagree
        }
        fields[name] = ToHex(o.bytes);
    };
    std::vector<int64_t> items = NumsAt(p.inventory, "items", 48, p.V);
    Out o;
    o.U16(NumAt(p.player, "rupees", p.P));
    put("rupees", o);
    o = Out();
    o.U16(NumAt(p.player, "health", p.P));
    put("health", o);
    o = Out();
    o.U8(NumAt(p.player, "magic", p.P));
    put("magic", o);
    o = Out();
    o.U16(NumAt(p.player, "swordHealth", p.P));
    put("swordHealth", o);
    o = Out();
    o.U8s(NumsAt(p.inventory, "ammo", 24, p.V));
    put("ammo", o);
    o = Out();
    for (int s = 0; s < kBottleSlots; s++) {
        o.U8(items[(size_t)(kSlotBottle1 + s)]);
    }
    put("bottleItems", o);
    o = Out();
    o.U8s(GridAt(p.equips, "buttonItems", 4, 4, p.E));
    put("buttonItems", o);
    o = Out();
    o.U8s(GridAt(p.equips, "cButtonSlots", 4, 4, p.E));
    put("cButtonSlots", o);
    o = Out();
    // 2 Ship's D-pad: saves older than it have none (empty, as 2 Ship's migration 5 leaves them)
    const json* dpad = nullptr;
    auto ship = p.save.find("shipSaveInfo");
    if (ship != p.save.end() && ship->is_object()) {
        auto it = ship->find("dpadEquips");
        dpad = it != ship->end() ? &*it : nullptr;
    }
    if (dpad != nullptr) {
        const char* where = "save.shipSaveInfo.dpadEquips";
        o.U8s(GridAt(*dpad, "dpadItems", 4, 4, where));
        o.U8s(GridAt(*dpad, "dpadSlots", 4, 4, where));
    } else {
        o.bytes.assign(32, kItemNone);
    }
    put("dpad", o);
    o = Out();
    o.U8(human ? kPlayerFormHuman : NumAt(p.save, "playerForm", p.S));
    put("form", o);
    o = Out();
    o.U8(NumAt(p.save, "equippedMask", p.S));
    put("mask", o);
    o = Out();
    o.U32s(NumsAt(p.info, "highScores", 7, p.I));
    put("highScores", o);
    o = Out();
    o.U32s(NumsAt(p.info, "dekuPlaygroundHighScores", 3, p.I));
    put("dekuScores", o);
    o = Out();
    o.U8s(GridAt(p.inventory, "dekuPlaygroundPlayerName", 3, 8, p.V));
    put("dekuNames", o);
    o = Out();
    o.U32(NumAt(p.info, "pictoFlags0", p.I));
    o.U32(NumAt(p.info, "pictoFlags1", p.I));
    put("picto", o);
    o = Out();
    o.U32(NumAt(p.info, "stolenItems", p.I));
    put("stolen", o);
    o = Out();
    o.U8(NumAt(p.info, "unk_F40", p.I));
    o.U8(NumAt(p.info, "scarecrowSpawnSongSet", p.I));
    o.U8s(NumsAt(p.info, "scarecrowSpawnSong", 128, p.I));
    put("scarecrow", o);
    o = Out();
    {
        std::string where = std::string(p.I) + ".horseData";
        const json& horse = Get(p.info, "horseData", p.I);
        const json& pos = Get(horse, "pos", where);
        o.U16(NumAt(horse, "sceneId", where));
        o.U16(NumAt(pos, "x", where + ".pos"));
        o.U16(NumAt(pos, "y", where + ".pos"));
        o.U16(NumAt(pos, "z", where + ".pos"));
        o.U16(NumAt(horse, "yaw", where));
    }
    put("horse", o);
    o = Out();
    o.U8(NumAt(p.info, "bombersCaughtNum", p.I));
    o.U8s(NumsAt(p.info, "bombersCaughtOrder", 5, p.I));
    put("bombers", o);
    return json{ { "v", 1 }, { "fields", fields } };
}

// newf == "ZELDA3": anything else is an empty or erased slot, as for the game.
bool HasNewf(const json& saveJson) {
    static const int64_t kZelda3[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    try {
        SaveParts p(saveJson);
        std::vector<int64_t> newf = NumsAt(p.player, "newf", 6, p.P);
        return std::equal(newf.begin(), newf.end(), kZelda3);
    } catch (const ImportError&) {
        return false;
    }
}

const json* PartOf(const json& file, const char* key) {
    auto part = file.find(key);
    if (part == file.end() || !part->is_object()) {
        return nullptr;
    }
    auto saveJson = part->find("save");
    return saveJson != part->end() && saveJson->is_object() ? &*saveJson : nullptr;
}

// Before version 4 a file held one Save: {"save": Save} (versions 1-3) or the Save itself (version 0).
const json* LegacyPart(const json& file) {
    if (file.contains("saveInfo")) {
        return &file;
    }
    auto saveJson = file.find("save");
    return saveJson != file.end() && saveJson->is_object() && saveJson->contains("saveInfo") ? &*saveJson : nullptr;
}

bool IsRando(const json& saveJson) {
    auto ship = saveJson.find("shipSaveInfo");
    return ship != saveJson.end() && ship->is_object() && GetInt(*ship, "saveType", 0) == kSaveTypeRando;
}

// Where Sram_OpenSave puts an owl save: its pause-save entrance (2 Ship's PauseSave), else its owl statue; the swamp's
// and the mountain's owls change scene with their temple cleared. False: none (the co-op's usual start).
bool OwlEntrance(const json& saveJson, const FieldBytes& world, uint16_t& out) {
    int64_t pause = -1;
    auto ship = saveJson.find("shipSaveInfo");
    if (ship != saveJson.end() && ship->is_object()) {
        pause = GetInt(*ship, "pauseSaveEntrance", -1);
    }
    uint16_t entrance = 0;
    if (pause >= 0 && pause <= 0xFFFF) {
        entrance = (uint16_t)pause;
    } else {
        int64_t owl = GetInt(saveJson, "owlSaveLocation", -1);
        if (owl < 0 || owl >= (int64_t)(sizeof(kOwlWarpEntrances) / sizeof(kOwlWarpEntrances[0]))) {
            return false;
        }
        entrance = kOwlWarpEntrances[owl];
    }
    const std::vector<uint8_t>& week = world[(size_t)world::FindField("weekEventReg")];
    auto cleared = [&week](uint16_t flag) { return (week[flag >> 8] & (flag & 0xFF)) != 0; };
    if (entrance == kEntranceSwampPoisonedOwl && cleared(kWeekClearedWoodfall)) {
        entrance = kEntranceSwampClearedOwl;
    } else if (entrance == kEntranceMountainWinterOwl && cleared(kWeekClearedSnowhead)) {
        entrance = kEntranceMountainSpringOwl;
    }
    out = entrance;
    return true;
}

// A new co-op player gets the Ocarina on C-down and the Deku Mask on C-left (SaveBuilder_LoadPlayer): an imported one
// too when the co-op minimum gave them and the button is empty.
void EquipBaseline(json& inv, uint32_t baseline) {
    json& fields = inv["fields"];
    std::vector<uint8_t> items;
    std::vector<uint8_t> slots;
    if (!FromHex(fields["buttonItems"].get<std::string>(), items) ||
        !FromHex(fields["cButtonSlots"].get<std::string>(), slots)) {
        return;
    }
    auto equip = [&items, &slots](int button, uint8_t item, int slot) {
        if (items[(size_t)button] == kItemNone) { // row 0: the human form's buttons
            items[(size_t)button] = item;
            slots[(size_t)button] = (uint8_t)slot;
        }
    };
    if (baseline & BaselineOcarina) {
        equip(kEquipSlotCDown, kItemOcarinaOfTime, kSlotOcarina);
    }
    if (baseline & BaselineDekuMask) {
        equip(kEquipSlotCLeft, kItemMaskDeku, kSlotMaskDeku);
    }
    fields["buttonItems"] = ToHex(items);
    fields["cButtonSlots"] = ToHex(slots);
}

const std::vector<uint8_t>& FieldOf(const FieldBytes& fields, const char* name) {
    return fields[(size_t)world::FindField(name)];
}

} // namespace

bool ParseSource(const std::string& text, ImportSource& out) {
    std::string lower = ToLower(text);
    if (lower == "auto") {
        out = ImportSource::Auto;
    } else if (lower == "buho" || lower == "búho" || lower == "owl") {
        out = ImportSource::Owl;
    } else if (lower == "ciclo" || lower == "cycle") {
        out = ImportSource::Cycle;
    } else {
        return false;
    }
    return true;
}

bool ImportSave(const json& file, const ImportOptions& opts, Imported& out, std::string* err) {
    auto fail = [err](const std::string& why) {
        if (err != nullptr) {
            *err = why;
        }
        return false;
    };
    if (!file.is_object() || (file.contains("type") && GetString(file, "type") != "2S2H_SAVE")) {
        return fail(Tr(Msg::ImpNotSave));
    }
    const json* cycle = PartOf(file, "newCycleSave");
    const json* owl = PartOf(file, "owlSave");
    if (cycle == nullptr && owl == nullptr) {
        cycle = LegacyPart(file);
    }
    if (cycle == nullptr && owl == nullptr) {
        return fail(Tr(Msg::ImpNotSave));
    }
    bool cycleOk = cycle != nullptr && HasNewf(*cycle);
    bool owlOk = owl != nullptr && HasNewf(*owl);
    const json* source = nullptr;
    switch (opts.source) {
        case ImportSource::Owl:
            if (!owlOk) {
                return fail(Tr(owl == nullptr ? Msg::ImpNoOwl : Msg::ImpEmpty));
            }
            source = owl;
            break;
        case ImportSource::Cycle:
            if (!cycleOk) {
                return fail(Tr(cycle == nullptr ? Msg::ImpNoCycle : Msg::ImpEmpty));
            }
            source = cycle;
            break;
        default:
            source = owlOk ? owl : (cycleOk ? cycle : nullptr);
            break;
    }
    if (source == nullptr) {
        return fail(Tr(Msg::ImpEmpty));
    }
    if (IsRando(*source) || (cycleOk && IsRando(*cycle))) {
        return fail(Tr(Msg::ImpRando));
    }
    Imported result;
    result.version = (int)GetInt(file, "version", 0);
    if (result.version > kKnownSaveVersion) {
        result.notes.push_back(Tr(Msg::ImpNewer, { std::to_string(result.version) }));
    }
    result.fromOwl = source == owl;
    try {
        result.fields = WorldOf(*source);
        result.inv = PlayerOf(*source, opts.loaderRules && !result.fromOwl);
        if (result.fromOwl && cycleOk) {
            result.start = WorldOf(*cycle);
            result.invStart = PlayerOf(*cycle, opts.loaderRules);
        } else {
            result.start = result.fields;
            result.invStart = result.inv;
        }
        if (result.fromOwl) {
            int64_t day = NumAt(*source, "day", "save");
            uint16_t time = (uint16_t)NumAt(*source, "time", "save");
            uint32_t latest = clock::kMoonAbs - clock::kClientMoonMargin - 60; // never on the moon's doorstep
            result.clockAbs = day < 1 ? 0 : std::min(clock::AbsOf((int)day, time), latest);
            result.inverted = NumAt(*source, "timeSpeedOffset", "save") == -2;
            uint16_t entrance = 0;
            if (OwlEntrance(*source, result.fields, entrance)) {
                result.inv["entrance"] = entrance;
            }
        }
        SaveParts parts(*source);
        result.playerName = DecodePlayerName(Get(parts.player, "playerName", parts.P));
    } catch (const ImportError& e) {
        return fail(Tr(Msg::ImpMissing, { e.path }));
    } catch (const json::exception& e) {
        return fail(Tr(Msg::ImpMissing, { e.what() }));
    }
    if (opts.baseline) {
        result.baseline = ApplyCoopBaseline(result.fields);
        uint32_t startAdded = ApplyCoopBaseline(result.start);
        EquipBaseline(result.inv, result.baseline);
        EquipBaseline(result.invStart, startAdded);
    }
    out = std::move(result);
    return true;
}

std::vector<std::string> DescribeImport(const Imported& imported) {
    std::vector<std::string> lines;
    std::string source = imported.fromOwl ? Tr(Msg::ImpSourceOwl, { clock::Format(imported.clockAbs) })
                                          : Tr(Msg::ImpSourceCycle);
    lines.push_back(Tr(Msg::ImpSumName, { imported.playerName.empty() ? "?" : imported.playerName,
                                          std::to_string(imported.version), source }));
    const std::vector<uint8_t>& hearts = FieldOf(imported.fields, "heartQuarters");
    int quarters = hearts[0] | (hearts[1] << 8);
    int masks = 0;
    for (uint8_t mask : FieldOf(imported.fields, "masks")) {
        masks += mask != kItemNone;
    }
    uint32_t quest = ReadU32(FieldOf(imported.fields, "quest").data());
    int songs = 0;
    for (int bit : kSongBits) {
        songs += (quest >> bit) & 1;
    }
    int remains = 0;
    for (int bit = 0; bit < 4; bit++) { // QUEST_REMAINS_ODOLWA..TWINMOLD
        remains += (quest >> bit) & 1;
    }
    const std::vector<uint8_t>& resets = FieldOf(imported.fields, "resets");
    lines.push_back(Tr(Msg::ImpSumStats, { std::to_string(quarters / 4), std::to_string(quarters % 4),
                                           std::to_string(masks), std::to_string(songs), std::to_string(remains),
                                           std::to_string(resets[0] | (resets[1] << 8)) }));
    if (imported.baseline != 0) {
        static const struct {
            uint32_t part;
            Msg name;
        } kParts[] = {
            { BaselineTatl, Msg::ImpAddTatl },         { BaselineOcarina, Msg::ImpAddOcarina },
            { BaselineDekuNut, Msg::ImpAddDekuNut },   { BaselineDekuMask, Msg::ImpAddDekuMask },
            { BaselineSongTime, Msg::ImpAddSongTime }, { BaselineSongHealing, Msg::ImpAddSongHealing },
            { BaselineMagic, Msg::ImpAddMagic },       { BaselineReset, Msg::ImpAddReset },
            { BaselineIntro, Msg::ImpAddIntro },
        };
        std::string names;
        for (const auto& part : kParts) {
            if (imported.baseline & part.part) {
                names += (names.empty() ? "" : ", ") + Tr(part.name);
            }
        }
        if (!names.empty()) {
            lines.push_back(Tr(Msg::ImpSumAdded, { names }));
        }
    }
    for (const std::string& note : imported.notes) {
        lines.push_back(note);
    }
    return lines;
}

std::string DecodePlayerName(const json& glyphs) {
    std::string name;
    if (!glyphs.is_array()) {
        return name;
    }
    for (const json& glyph : glyphs) {
        int64_t c = glyph.is_number_integer() ? glyph.get<int64_t>() : -1;
        char ch = ' ';
        if (c >= 0 && c <= 9) {
            ch = (char)('0' + c);
        } else if (c >= 0x0A && c <= 0x23) {
            ch = (char)('A' + (c - 0x0A));
        } else if (c >= 0x24 && c <= 0x3D) {
            ch = (char)('a' + (c - 0x24));
        }
        name.push_back(ch);
    }
    size_t first = name.find_first_not_of(' ');
    if (first == std::string::npos) {
        return "";
    }
    return name.substr(first, name.find_last_not_of(' ') - first + 1);
}

json FieldsToJson(const FieldBytes& fields) {
    json out = json::object();
    for (size_t i = 0; i < world::kFieldCount && i < fields.size(); i++) {
        out[world::kFields[i].name] = ToHex(fields[i]);
    }
    return out;
}

} // namespace coop::save

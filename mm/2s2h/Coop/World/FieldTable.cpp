#include "FieldTable.h"

#include "2s2h/Coop/Actors/LiveFlags.h"
#include "2s2h/Coop/Sync/Sync.h"

#include "common/Hex.h"
#include "common/WorldFields.h"
#include "common/WorldRules.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client::fields {

namespace {

using coop::client::LiveFlagType;
using coop::client::LiveFlags_OnRemoteFlag;

constexpr int kScenes = 120;
static_assert(sizeof(gSaveContext.cycleSceneFlags) == kScenes * sizeof(CycleSceneFlags), "cycleSceneFlags");
static_assert(sizeof(gSaveContext.save.saveInfo.permanentSceneFlags) == kScenes * sizeof(PermanentSceneFlags),
              "permanentSceneFlags");
static_assert(SLOT_MASK_POSTMAN == 24 && SLOT_BOTTLE_1 == 18 && SLOT_BOTTLE_6 == 23, "inventory layout");

constexpr size_t kCodesSize =
    offsetof(SaveInfo, bomberCode) + sizeof(SaveInfo::bomberCode) - offsetof(SaveInfo, lotteryCodes);
static_assert(kCodesSize == 20, "lotteryCodes, spiderHouseMaskOrder and bomberCode must be contiguous");
static_assert(offsetof(SaveInfo, pictoFlags1) == offsetof(SaveInfo, pictoFlags0) + 4, "pictoFlags");
static_assert(offsetof(SaveInfo, scarecrowSpawnSong) + 128 - offsetof(SaveInfo, unk_F40) == 130, "scarecrow");
static_assert(offsetof(SaveInfo, bombersCaughtOrder) + 5 - offsetof(SaveInfo, bombersCaughtNum) == 6, "bombers");
// coop/common/WorldRules.h mirrors these (the sword is the low nibble of equipment: gEquipShifts[EQUIP_TYPE_SWORD] 0)
static_assert(world::kItemSwordGreatFairy == ITEM_SWORD_GREAT_FAIRY && world::kItemSwordGilded == ITEM_SWORD_GILDED &&
                  world::kSlotSwordGreatFairy == SLOT_SWORD_GREAT_FAIRY &&
                  world::kEquipSwordRazor == EQUIP_VALUE_SWORD_RAZOR &&
                  world::kEquipSwordGilded == EQUIP_VALUE_SWORD_GILDED,
              "WorldRules.h");

SaveInfo& Info() {
    return gSaveContext.save.saveInfo;
}

// Engine bits that belong to this game only: "a scene is loaded" (92_80), the state of Gorman's horse race
// (92 & 7: a value, not progress, so each game runs its own race) and the leftovers the map select also clears when
// it loads a save (z_select.c).
constexpr uint16_t kLocalWeekEventFlags[] = {
    WEEKEVENTREG_92_80, PACK_WEEKEVENTREG_FLAG(92, WEEKEVENTREG_HORSE_RACE_STATE_MASK),
    WEEKEVENTREG_08_01, WEEKEVENTREG_KICKOUT_WAIT,
    WEEKEVENTREG_82_08, WEEKEVENTREG_90_20,
};

uint8_t LocalWeekEventMask(int index) {
    uint8_t mask = 0;
    for (uint16_t flag : kLocalWeekEventFlags) {
        if ((flag >> 8) == index) {
            mask |= (uint8_t)(flag & 0xFF);
        }
    }
    return mask;
}

void ReadWeekEvents(uint8_t* out) {
    for (int i = 0; i < 100; i++) {
        out[i] = (uint8_t)(Info().weekEventReg[i] & ~LocalWeekEventMask(i));
    }
}

void WriteWeekEvents(const uint8_t* in) {
    for (int i = 0; i < 100; i++) {
        uint8_t local = LocalWeekEventMask(i);
        Info().weekEventReg[i] = (uint8_t)((Info().weekEventReg[i] & local) | (in[i] & ~local));
    }
}

// The loaded scene keeps its flags in actorCtx.sceneFlags (the same rules as Play_SaveCycleSceneFlags).
// Bits that a remote change turned on in the loaded scene: the actors that only read their flag when created are
// removed at once (sub-project C, LiveFlags.cpp). Flag 0 means "no flag" for collectibles.
void NotifyNewBits(LiveFlagType type, uint32_t before, uint32_t after, int base) {
    uint32_t added = after & ~before;
    for (int bit = 0; bit < 32; bit++) {
        int flag = base + bit;
        if ((added & (1u << bit)) && !(type == LiveFlagType::Collectible && flag == 0)) {
            LiveFlags_OnRemoteFlag(type, flag);
        }
    }
}

void LoadCurrentSceneFlags(PlayState* play) {
    ActorContextSceneFlags before = play->actorCtx.sceneFlags;
    CycleSceneFlags* flags = &gSaveContext.cycleSceneFlags[Play_GetOriginalSceneId(play->sceneId)];
    play->actorCtx.sceneFlags.chest = flags->chest;
    play->actorCtx.sceneFlags.switches[0] = flags->switch0;
    play->actorCtx.sceneFlags.switches[1] = flags->switch1;
    if (play->sceneId == SCENE_INISIE_R) {
        flags = &gSaveContext.cycleSceneFlags[play->sceneId];
    }
    play->actorCtx.sceneFlags.collectible[0] = flags->collectible;
    play->actorCtx.sceneFlags.clearedRoom = flags->clearedRoom;

    const ActorContextSceneFlags& after = play->actorCtx.sceneFlags;
    NotifyNewBits(LiveFlagType::Chest, before.chest, after.chest, 0);
    NotifyNewBits(LiveFlagType::Switch, before.switches[0], after.switches[0], 0);
    NotifyNewBits(LiveFlagType::Switch, before.switches[1], after.switches[1], 32);
    NotifyNewBits(LiveFlagType::Collectible, before.collectible[0], after.collectible[0], 0);
    coop::client::SceneFlags_NoteWorldWrite(before); // never sent again as this scene's; reloads (Sync/SceneFlags.cpp)
}

void ReadSceneFlags(uint8_t* out) {
    if (PlayLive()) {
        Play_SaveCycleSceneFlags(gPlayState);
    }
    std::memcpy(out, gSaveContext.cycleSceneFlags, sizeof(gSaveContext.cycleSceneFlags));
}

void WriteSceneFlags(const uint8_t* in) {
    std::memcpy(gSaveContext.cycleSceneFlags, in, sizeof(gSaveContext.cycleSceneFlags));
    if (PlayLive()) {
        LoadCurrentSceneFlags(gPlayState); // actors already spawned only react when the scene reloads
    }
}

template <u32 PermanentSceneFlags::*Member> void ReadPermanent(uint8_t* out) {
    for (int i = 0; i < kScenes; i++) {
        std::memcpy(out + i * 4, &(Info().permanentSceneFlags[i].*Member), 4);
    }
}

template <u32 PermanentSceneFlags::*Member> void WritePermanent(const uint8_t* in) {
    for (int i = 0; i < kScenes; i++) {
        std::memcpy(&(Info().permanentSceneFlags[i].*Member), in + i * 4, 4);
    }
}

// Heart pieces travel in heartQuarters; the pictograph is each player's own photo.
constexpr uint32_t kLocalQuestBits = 0xF0000000u | (1u << QUEST_PICTOGRAPH);

void ReadQuest(uint8_t* out) {
    uint32_t value = Info().inventory.questItems & ~kLocalQuestBits;
    std::memcpy(out, &value, 4);
}

void WriteQuest(const uint8_t* in) {
    uint32_t value = 0;
    std::memcpy(&value, in, 4);
    u32& quest = Info().inventory.questItems;
    quest = (quest & kLocalQuestBits) | (value & ~kLocalQuestBits);
}

// An inventory slot only takes an item that belongs to it (gItemSlots) or ITEM_NONE.
void WriteSlots(const uint8_t* in, int firstSlot, int count) {
    for (int i = 0; i < count; i++) {
        int slot = firstSlot + i;
        uint8_t item = in[i];
        bool fits = item == ITEM_NONE || (item < std::size(gItemSlots) && gItemSlots[item] == slot);
        if (!fits || Info().inventory.items[slot] == item) {
            continue;
        }
        Info().inventory.items[slot] = item;
        RefreshButtonsForSlot(slot);
    }
}

void ReadItems(uint8_t* out) {
    std::memcpy(out, &Info().inventory.items[0], SLOT_BOTTLE_1);
}

void WriteItems(const uint8_t* in) {
    WriteSlots(in, 0, SLOT_BOTTLE_1);
}

void ReadMasks(uint8_t* out) {
    std::memcpy(out, &Info().inventory.items[SLOT_MASK_POSTMAN], 24);
}

void WriteMasks(const uint8_t* in) {
    WriteSlots(in, SLOT_MASK_POSTMAN, 24);
}

void ReadEquipment(uint8_t* out) {
    std::memcpy(out, &Info().equips.equipment, 2);
}

void WriteEquipment(const uint8_t* in) {
    u16 before = Info().equips.equipment;
    u16 swordBefore = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    std::memcpy(&Info().equips.equipment, in, 2);
    if (GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_MAX ||
        GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) >= EQUIP_VALUE_SHIELD_MAX) {
        Info().equips.equipment = before; // not a sword or shield of this game
        return;
    }
    u16 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    if (sword != swordBefore) {
        if (sword == EQUIP_VALUE_SWORD_RAZOR) {
            Info().playerData.swordHealth = 100; // a new Razor Sword, as when the smith hands it over
        }
        SyncSwordButton();
    }
}

void ReadMagicFlags(uint8_t* out) {
    out[0] = Info().playerData.isMagicAcquired;
    out[1] = Info().playerData.isDoubleMagicAcquired;
}

void WriteMagicFlags(const uint8_t* in) {
    SavePlayerData& player = Info().playerData;
    uint8_t magic = in[0] != 0;
    uint8_t doubleMagic = in[1] != 0;
    if (magic == player.isMagicAcquired && doubleMagic == player.isDoubleMagicAcquired) {
        return;
    }
    player.isMagicAcquired = magic;
    player.isDoubleMagicAcquired = doubleMagic;
    // Full meter of the new size: the game rebuilds it when magicLevel is 0 (as the debug upgrade does).
    player.magic = !magic ? 0 : (doubleMagic ? MAGIC_DOUBLE_METER : MAGIC_NORMAL_METER);
    player.magicLevel = 0;
}

void ReadDefense(uint8_t* out) {
    out[0] = Info().playerData.doubleDefense;
}

void WriteDefense(const uint8_t* in) {
    Info().playerData.doubleDefense = in[0] != 0;
    // The hearts drawn with a white border (SkipGreatFairyCutscene.cpp sets the same 20)
    Info().inventory.defenseHearts = Info().playerData.doubleDefense ? 20 : 0;
}

void ReadProgress(uint8_t* out) {
    out[0] = gSaveContext.save.isFirstCycle;
    out[1] = gSaveContext.save.snowheadCleared;
    out[2] = gSaveContext.save.hasTatl;
}

void WriteProgress(const uint8_t* in) {
    gSaveContext.save.isFirstCycle = in[0];
    gSaveContext.save.snowheadCleared = in[1];
    gSaveContext.save.hasTatl = in[2];
}

void ReadCodes(uint8_t* out) {
    std::memcpy(out, Info().lotteryCodes, kCodesSize);
}

void WriteCodes(const uint8_t* in) {
    std::memcpy(Info().lotteryCodes, in, kCodesSize);
}

// Max health in quarter hearts: containers * 4 + heart pieces (3 hearts minimum, 20 hearts + 3 pieces maximum).
void ReadHeartQuarters(uint8_t* out) {
    int quarters = Info().playerData.healthCapacity / 0x10 * 4 + (int)GET_QUEST_HEART_PIECE_COUNT;
    out[0] = (uint8_t)(quarters & 0xFF);
    out[1] = (uint8_t)(quarters >> 8);
}

void WriteHeartQuarters(const uint8_t* in) {
    int quarters = std::clamp(in[0] | (in[1] << 8), 12, 83);
    SavePlayerData& player = Info().playerData;
    s16 before = player.healthCapacity;
    player.healthCapacity = (s16)(quarters / 4 * 0x10);
    Info().inventory.questItems =
        (Info().inventory.questItems & ~0xF0000000u) | ((u32)(quarters % 4) << QUEST_HEART_PIECE_COUNT);
    if (player.healthCapacity > before) {
        player.health += player.healthCapacity - before; // a new container heals, as when picking it up
    }
    player.health = std::min(player.health, player.healthCapacity);
}

void ReadBottles(uint8_t* out) {
    out[0] = (uint8_t)BottleCount();
}

void WriteBottles(const uint8_t* in) {
    SetBottleCount(in[0]);
}

struct Access {
    const char* name;
    uint8_t* (*memory)(); // plain bytes of the save (nullptr: read/write below)
    size_t size;
    void (*read)(uint8_t* out);
    void (*write)(const uint8_t* in);
};

#define SAVE_BYTES(name, lvalue) { name, [] { return (uint8_t*)&(lvalue); }, sizeof(lvalue), nullptr, nullptr }
#define CUSTOM(name, size, read, write) { name, nullptr, size, read, write }

// Same order and sizes as coop::world::kFields (checked below).
constexpr Access kAccess[] = {
    CUSTOM("weekEventReg", 100, ReadWeekEvents, WriteWeekEvents),
    CUSTOM("sceneFlags", 2400, ReadSceneFlags, WriteSceneFlags),
    CUSTOM("sceneRooms", 480, ReadPermanent<&PermanentSceneFlags::rooms>, WritePermanent<&PermanentSceneFlags::rooms>),
    SAVE_BYTES("owls", gSaveContext.save.saveInfo.playerData.owlActivationFlags),
    SAVE_BYTES("mapsVisible", gSaveContext.save.saveInfo.scenesVisible),
    SAVE_BYTES("regions", gSaveContext.save.saveInfo.regionsVisited),
    SAVE_BYTES("clouds", gSaveContext.save.saveInfo.worldMapCloudVisibility),
    SAVE_BYTES("gossipHearts", gSaveContext.save.saveInfo.unk_EA0),
    SAVE_BYTES("blueWarps", gSaveContext.save.saveInfo.unk_EA8),
    SAVE_BYTES("upgrades", gSaveContext.save.saveInfo.inventory.upgrades),
    CUSTOM("quest", 4, ReadQuest, WriteQuest),
    SAVE_BYTES("dungeonItems", gSaveContext.save.saveInfo.inventory.dungeonItems),
    CUSTOM("items", 18, ReadItems, WriteItems),
    CUSTOM("masks", 24, ReadMasks, WriteMasks),
    CUSTOM("equipment", 2, ReadEquipment, WriteEquipment),
    CUSTOM("magicFlags", 2, ReadMagicFlags, WriteMagicFlags),
    CUSTOM("defense", 1, ReadDefense, WriteDefense),
    CUSTOM("progress", 3, ReadProgress, WriteProgress),
    SAVE_BYTES("resets", gSaveContext.save.saveInfo.playerData.threeDayResetCount),
    CUSTOM("codes", 20, ReadCodes, WriteCodes),
    CUSTOM("heartQuarters", 2, ReadHeartQuarters, WriteHeartQuarters),
    CUSTOM("bottles", 1, ReadBottles, WriteBottles),
    SAVE_BYTES("keys", gSaveContext.save.saveInfo.inventory.dungeonKeys),
    SAVE_BYTES("fairies", gSaveContext.save.saveInfo.inventory.strayFairies),
    SAVE_BYTES("skulls", gSaveContext.save.saveInfo.skullTokenCount),
    CUSTOM("sceneExtra", 480, ReadPermanent<&PermanentSceneFlags::unk_14>, WritePermanent<&PermanentSceneFlags::unk_14>),
};

#undef SAVE_BYTES
#undef CUSTOM

constexpr bool SameName(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

constexpr bool TableMatchesSchema() {
    if (std::size(kAccess) != world::kFieldCount) {
        return false;
    }
    for (size_t i = 0; i < world::kFieldCount; i++) {
        if (!SameName(kAccess[i].name, world::kFields[i].name) || kAccess[i].size != world::kFields[i].size) {
            return false;
        }
    }
    return true;
}
static_assert(TableMatchesSchema(), "kAccess must list coop::world::kFields in the same order with the same sizes");

struct PlayerField {
    const char* name;
    uint8_t* (*memory)();
    size_t size;
};

#define PLAYER_BYTES(name, lvalue) { name, [] { return (uint8_t*)&(lvalue); }, sizeof(lvalue) }
#define PLAYER_RANGE(name, first, size) { name, [] { return (uint8_t*)&(first); }, size }

// Everything of a player that the world does not share. Add new entries at the end.
constexpr PlayerField kPlayerFields[] = {
    PLAYER_BYTES("rupees", gSaveContext.save.saveInfo.playerData.rupees),
    PLAYER_BYTES("health", gSaveContext.save.saveInfo.playerData.health),
    PLAYER_BYTES("magic", gSaveContext.save.saveInfo.playerData.magic),
    PLAYER_BYTES("swordHealth", gSaveContext.save.saveInfo.playerData.swordHealth),
    PLAYER_BYTES("ammo", gSaveContext.save.saveInfo.inventory.ammo),
    PLAYER_RANGE("bottleItems", gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1], 6),
    PLAYER_BYTES("buttonItems", gSaveContext.save.saveInfo.equips.buttonItems),
    PLAYER_BYTES("cButtonSlots", gSaveContext.save.saveInfo.equips.cButtonSlots),
    PLAYER_BYTES("dpad", gSaveContext.save.shipSaveInfo.dpadEquips),
    PLAYER_BYTES("form", gSaveContext.save.playerForm),
    PLAYER_BYTES("mask", gSaveContext.save.equippedMask),
    PLAYER_BYTES("highScores", gSaveContext.save.saveInfo.highScores), // includes the bank
    PLAYER_BYTES("dekuScores", gSaveContext.save.saveInfo.dekuPlaygroundHighScores),
    PLAYER_BYTES("dekuNames", gSaveContext.save.saveInfo.inventory.dekuPlaygroundPlayerName),
    PLAYER_RANGE("picto", gSaveContext.save.saveInfo.pictoFlags0, 8),
    PLAYER_BYTES("stolen", gSaveContext.save.saveInfo.stolenItems),
    PLAYER_RANGE("scarecrow", gSaveContext.save.saveInfo.unk_F40, 130),
    PLAYER_BYTES("horse", gSaveContext.save.saveInfo.horseData),
    PLAYER_RANGE("bombers", gSaveContext.save.saveInfo.bombersCaughtNum, 6),
};

#undef PLAYER_BYTES
#undef PLAYER_RANGE

bool IsButtonItem(uint8_t item) {
    return item == ITEM_NONE || item == ITEM_FD || item < std::size(gItemIcons);
}

bool IsSlot(uint8_t slot) {
    return slot == SLOT_NONE || slot < SLOT_MASK_POSTMAN + 24;
}

// Player data comes from the server: nothing in it may point outside the engine's tables.
void SanitizePlayer() {
    Save& save = gSaveContext.save;
    if (save.playerForm >= PLAYER_FORM_MAX) {
        save.playerForm = PLAYER_FORM_HUMAN;
    }
    if (save.equippedMask >= PLAYER_MASK_MAX) {
        save.equippedMask = PLAYER_MASK_NONE;
    }
    for (auto& row : Info().equips.buttonItems) {
        for (u8& item : row) {
            item = IsButtonItem(item) ? item : ITEM_NONE;
        }
    }
    for (auto& row : Info().equips.cButtonSlots) {
        for (u8& slot : row) {
            slot = IsSlot(slot) ? slot : SLOT_NONE;
        }
    }
    for (auto& row : save.shipSaveInfo.dpadEquips.dpadItems) {
        for (u8& item : row) {
            item = IsButtonItem(item) ? item : ITEM_NONE;
        }
    }
    for (auto& row : save.shipSaveInfo.dpadEquips.dpadSlots) {
        for (u8& slot : row) {
            slot = IsSlot(slot) ? slot : SLOT_NONE;
        }
    }
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        u8& item = Info().inventory.items[slot];
        if (item != ITEM_NONE && (item < ITEM_BOTTLE || item > ITEM_OBABA_DRINK)) {
            item = ITEM_BOTTLE;
        }
    }
}

bool DecodeField(const json& fields, size_t i, std::vector<uint8_t>& out) {
    auto it = fields.find(world::kFields[i].name);
    return it != fields.end() && it->is_string() && FromHex(it->get<std::string>(), out) &&
           out.size() == world::kFields[i].size;
}

} // namespace

void Read(int field, uint8_t* out) {
    const Access& a = kAccess[field];
    if (a.memory != nullptr) {
        std::memcpy(out, a.memory(), a.size);
    } else {
        a.read(out);
    }
}

void Write(int field, const uint8_t* in) {
    const Access& a = kAccess[field];
    if (a.memory != nullptr) {
        std::memcpy(a.memory(), in, a.size);
    } else {
        a.write(in);
    }
}

json ReadAllJson() {
    json out = json::object();
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < world::kFieldCount; i++) {
        bytes.assign(world::kFields[i].size, 0);
        Read((int)i, bytes.data());
        out[world::kFields[i].name] = ToHex(bytes);
    }
    return out;
}

bool CheckJson(const json& fields, std::string* err) {
    if (!fields.is_object()) {
        if (err != nullptr) {
            *err = "faltan los campos del mundo";
        }
        return false;
    }
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < world::kFieldCount; i++) {
        if (!DecodeField(fields, i, bytes)) {
            if (err != nullptr) {
                *err = std::string("campo '") + world::kFields[i].name + "' ausente o dañado";
            }
            return false;
        }
    }
    return true;
}

bool WriteAllJson(const json& fields, std::string* err) {
    if (!CheckJson(fields, err)) {
        return false;
    }
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < world::kFieldCount; i++) {
        DecodeField(fields, i, bytes);
        Write((int)i, bytes.data());
    }
    return true;
}

json ReadPlayer() {
    json out = json::object();
    for (const PlayerField& f : kPlayerFields) {
        out[f.name] = ToHex(f.memory(), f.size);
    }
    return out;
}

void WritePlayer(const json& fields) {
    if (!fields.is_object()) {
        return;
    }
    std::vector<uint8_t> bytes;
    for (const PlayerField& f : kPlayerFields) {
        auto it = fields.find(f.name);
        if (it == fields.end() || !it->is_string() || !FromHex(it->get<std::string>(), bytes) ||
            bytes.size() != f.size) {
            continue;
        }
        std::memcpy(f.memory(), bytes.data(), f.size);
    }
    SanitizePlayer();
}

bool PlayLive() {
    // WEEKEVENTREG_92_80 is set by Actor_InitContext and cleared by Play_Destroy before its hooks run
    return gPlayState != nullptr && (Info().weekEventReg[92] & 0x80) != 0;
}

int BottleCount() {
    int count = 0;
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6; slot++) {
        count += Info().inventory.items[slot] != ITEM_NONE;
    }
    count += STOLEN_ITEM_1 == ITEM_BOTTLE;
    count += STOLEN_ITEM_2 == ITEM_BOTTLE;
    return count;
}

void SetBottleCount(int count) {
    count = std::clamp(count, 0, SLOT_BOTTLE_6 - SLOT_BOTTLE_1 + 1);
    int have = BottleCount();
    u8* items = Info().inventory.items;
    for (int slot = SLOT_BOTTLE_1; slot <= SLOT_BOTTLE_6 && have < count; slot++) {
        if (items[slot] == ITEM_NONE) {
            items[slot] = ITEM_BOTTLE;
            have++;
        }
    }
    for (int pass = 0; pass < 2 && have > count; pass++) { // empty bottles first, then any
        for (int slot = SLOT_BOTTLE_6; slot >= SLOT_BOTTLE_1 && have > count; slot--) {
            if (items[slot] != ITEM_NONE && (pass == 1 || items[slot] == ITEM_BOTTLE)) {
                items[slot] = ITEM_NONE;
                RefreshButtonsForSlot(slot);
                have--;
            }
        }
    }
}

// As Interface_Init: only buttons that hold an item. For an empty D-pad button Interface_Dpad_LoadItemIconImpl
// writes the empty icon over a B / C button's.
static void LoadDpadIcon(int btn) {
    if (CVarGetInteger("gEnhancements.Dpad.DpadEquips", 0) && DPAD_BUTTON_ITEM_EQUIP(0, btn) < ITEM_F0) {
        Interface_Dpad_LoadItemIconImpl(gPlayState, btn);
    }
}

void RefreshButtonsForSlot(int slot) {
    u8 item = Info().inventory.items[slot];
    bool live = PlayLive();
    for (int btn = EQUIP_SLOT_C_LEFT; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (C_SLOT_EQUIP(0, btn) != slot) {
            continue;
        }
        BUTTON_ITEM_EQUIP(0, btn) = item;
        if (item == ITEM_NONE) {
            C_SLOT_EQUIP(0, btn) = SLOT_NONE;
        }
        if (live) {
            Interface_LoadItemIconImpl(gPlayState, btn);
        }
    }
    for (int btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        if (DPAD_SLOT_EQUIP(0, btn) != slot) {
            continue;
        }
        DPAD_BUTTON_ITEM_EQUIP(0, btn) = item;
        if (item == ITEM_NONE) {
            DPAD_SLOT_EQUIP(0, btn) = SLOT_NONE;
        }
        if (live) {
            LoadDpadIcon(btn);
        }
    }
}

void LoadButtonIcons() {
    for (int btn = EQUIP_SLOT_B; btn <= EQUIP_SLOT_C_RIGHT; btn++) {
        if (BUTTON_ITEM_EQUIP(0, btn) < ITEM_F0) {
            Interface_LoadItemIconImpl(gPlayState, btn);
        }
    }
    for (int btn = EQUIP_SLOT_D_RIGHT; btn <= EQUIP_SLOT_D_UP; btn++) {
        LoadDpadIcon(btn);
    }
}

void SyncSwordButton() {
    u8& b = BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_B);
    if (b != ITEM_NONE && (b < ITEM_SWORD_KOKIRI || b > ITEM_SWORD_GILDED)) {
        return; // a minigame put its own item on B (the bow of the shooting gallery...)
    }
    u16 sword = GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    if (sword > EQUIP_VALUE_SWORD_GILDED) {
        return;
    }
    u8 item = sword == EQUIP_VALUE_SWORD_NONE ? (u8)ITEM_NONE : (u8)(ITEM_SWORD_KOKIRI + sword - EQUIP_VALUE_SWORD_KOKIRI);
    if (b == item) {
        return;
    }
    b = item;
    if (PlayLive() && CUR_FORM == 0) {
        Interface_LoadItemIconImpl(gPlayState, EQUIP_SLOT_B);
    }
}

std::string SelfTest() {
    json shared = ReadAllJson();
    json player = ReadPlayer();
    std::string err;
    if (!WriteAllJson(shared, &err)) {
        return "WriteAllJson: " + err;
    }
    WritePlayer(player);
    json sharedAgain = ReadAllJson();
    for (size_t i = 0; i < world::kFieldCount; i++) {
        const char* name = world::kFields[i].name;
        if (shared[name] != sharedAgain[name]) {
            return std::string("el campo del mundo '") + name + "' cambia al escribirlo";
        }
    }
    json playerAgain = ReadPlayer();
    for (const PlayerField& f : kPlayerFields) {
        if (player[f.name] != playerAgain[f.name]) {
            return std::string("el campo del jugador '") + f.name + "' cambia al escribirlo";
        }
    }
    return "";
}

} // namespace coop::client::fields

#pragma once
// The game's save as the co-op sees it outside the game (the save converter, the server): constants mirrored from
// the game's headers, the schema of each player's own data and the starting state of a co-op world.
// mm/2s2h/Coop/World/FieldTable.cpp checks every constant and size here against the engine at compile time, so a
// mismatch does not build. New player datum: one line at the END of kPlayerFields (and of FieldTable's kPlayerAccess).
#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop::save {

// ---- Inventory (inventory.items: 24 item slots, then 24 masks) ----
inline constexpr uint8_t kItemNone = 0xFF;          // ITEM_NONE
inline constexpr uint8_t kItemOcarinaOfTime = 0x00; // ITEM_OCARINA_OF_TIME
inline constexpr uint8_t kItemDekuNut = 0x09;       // ITEM_DEKU_NUT
inline constexpr uint8_t kItemBottle = 0x12;        // ITEM_BOTTLE
inline constexpr uint8_t kItemMaskDeku = 0x32;      // ITEM_MASK_DEKU
inline constexpr int kSlotOcarina = 0;              // SLOT_OCARINA
inline constexpr int kSlotDekuNut = 9;              // SLOT_DEKU_NUT
inline constexpr int kItemFieldSlots = 18;          // the world's "items": slots 0..17 (bottles are the players')
inline constexpr int kSlotBottle1 = 18;             // SLOT_BOTTLE_1
inline constexpr int kBottleSlots = 6;
inline constexpr int kSlotMaskFirst = 24;           // SLOT_MASK_POSTMAN
inline constexpr int kMaskSlots = 24;
inline constexpr int kSlotMaskDeku = 29;            // SLOT_MASK_DEKU

// ---- inventory.questItems ----
inline constexpr int kQuestSongTime = 12;        // QUEST_SONG_TIME
inline constexpr int kQuestSongHealing = 13;     // QUEST_SONG_HEALING
inline constexpr int kQuestPictograph = 25;      // QUEST_PICTOGRAPH
inline constexpr int kQuestHeartPieceShift = 28; // QUEST_HEART_PIECE_COUNT: 4 bits of loose heart pieces
// Heart pieces travel in the world's heartQuarters; the pictograph is each player's own photo.
inline constexpr uint32_t kLocalQuestBits = 0xF0000000u | (1u << kQuestPictograph);
inline constexpr int kQuestSongSaria = 11; // QUEST_SONG_SARIA: in the engine, never given in Majora's Mask
inline constexpr int kQuestSongSun = 17;   // QUEST_SONG_SUN: the same

// ---- inventory.upgrades: UPG_QUIVER .. UPG_DEKU_NUTS, 2-3 bits each, 0..3 index gUpgradeCapacities ----
inline constexpr int kUpgradeCount = 8;
inline constexpr int kUpgradeShifts[kUpgradeCount] = { 0, 3, 6, 9, 12, 14, 17, 20 }; // gUpgradeShifts
inline constexpr int kUpgradeBits[kUpgradeCount] = { 3, 3, 3, 3, 2, 3, 3, 3 };       // gUpgradeMasks' widths
inline constexpr uint32_t kNewSaveUpgrades = (1u << 17) | (1u << 20); // Sram_InitNewSave: sticks 1, nuts 1, rest 0

// What the first /unlockall left that no save can have (the new one repairs it): upgrade values past
// gUpgradeCapacities' four columns (it wrote Deku sticks and nuts 7, bullet bag 4) go back to a new save's, and the
// unused songs of Saria and the Sun go away. Everything else stays as it is.
uint32_t RepairUpgrades(uint32_t upgrades);
uint32_t RepairQuestItems(uint32_t questItems);

// ---- weekEventReg flags, packed (index << 8) | mask like the game's PACK_WEEKEVENTREG_FLAG ----
constexpr uint16_t WeekFlag(int index, int mask) {
    return (uint16_t)((index << 8) | mask);
}
// Engine bits that belong to each game only: "a scene is loaded" (92_80), the state of Gorman's horse race (92 & 7:
// a value, not progress, so each game runs its own race) and the minigame leftovers the map select also clears.
inline constexpr uint16_t kLocalWeekEventFlags[] = {
    WeekFlag(92, 0x80), WeekFlag(92, 0x07), WeekFlag(8, 0x01),
    WeekFlag(63, 0x01), WeekFlag(82, 0x08), WeekFlag(90, 0x20),
};
uint8_t LocalWeekEventMask(int index); // the bits of weekEventReg[index] that never leave a game
inline constexpr uint16_t kWeekClearedWoodfall = WeekFlag(20, 0x02); // WEEKEVENTREG_CLEARED_WOODFALL_TEMPLE
inline constexpr uint16_t kWeekClearedSnowhead = WeekFlag(33, 0x80); // WEEKEVENTREG_CLEARED_SNOWHEAD_TEMPLE
// What SkipIntroSequence leaves: the intro's 59_04 and 31_04, East/West/North Clock Town already entered.
inline constexpr uint16_t kBaselineWeekEvents[] = {
    WeekFlag(59, 0x04), WeekFlag(31, 0x04), WeekFlag(2, 0x08), WeekFlag(2, 0x10), WeekFlag(2, 0x20),
};

// ---- Scenes (cycleSceneFlags[120]: chest, switch0, switch1, clearedRoom, collectible as u32) ----
inline constexpr int kSceneCount = 120;
inline constexpr int kCycleSceneFlagsSize = 20; // sizeof(CycleSceneFlags)
inline constexpr int kCycleChest = 0;           // offsetof(CycleSceneFlags, chest)
inline constexpr int kCycleSwitch0 = 4;         // offsetof(CycleSceneFlags, switch0)
inline constexpr int kSceneClockTowerInterior = 0x63; // SCENE_INSIDETOWER
inline constexpr int kSceneOpeningDungeon = 0x1A;     // SCENE_OPENINGDAN
inline constexpr int kSceneFairyFountain = 0x26;      // SCENE_YOUSEI_IZUMI

// ---- Entrances: the game's ENTRANCE(scene, spawn) with the scene's entrance table index ----
constexpr uint16_t Entrance(int entranceScene, int spawn) {
    return (uint16_t)(((entranceScene & 0x7F) << 9) | ((spawn & 0x1F) << 4));
}
// Where Sram_OpenSave puts an owl save, by its owlWarpId (OWL_WARP_GREAT_BAY_COAST .. OWL_WARP_STONE_TOWER).
inline constexpr uint16_t kOwlWarpEntrances[10] = {
    Entrance(0x34, 11), // Great Bay Coast
    Entrance(0x35, 6),  // Zora Cape
    Entrance(0x59, 3),  // Snowhead
    Entrance(0x4D, 8),  // Mountain Village (winter)
    Entrance(0x6C, 9),  // South Clock Town
    Entrance(0x1F, 4),  // Milk Road
    Entrance(0x43, 4),  // Woodfall
    Entrance(0x42, 10), // Southern Swamp (poisoned)
    Entrance(0x10, 4),  // Ikana Canyon
    Entrance(0x55, 3),  // Stone Tower
};
// The two owls whose scene changes with a cleared temple (Sram_OpenSave does the same swap).
inline constexpr uint16_t kEntranceSwampPoisonedOwl = Entrance(0x42, 10);
inline constexpr uint16_t kEntranceSwampClearedOwl = Entrance(0x06, 10);
inline constexpr uint16_t kEntranceMountainWinterOwl = Entrance(0x4D, 8);
inline constexpr uint16_t kEntranceMountainSpringOwl = Entrance(0x57, 8);

// ---- Player ----
inline constexpr uint8_t kPlayerFormHuman = 4; // PLAYER_FORM_HUMAN
inline constexpr int kEquipSlotCLeft = 1;      // EQUIP_SLOT_C_LEFT
inline constexpr int kEquipSlotCDown = 2;      // EQUIP_SLOT_C_DOWN

// Each player's own data: the "inv" the server keeps for every player (what the world does not share). The game maps
// every entry to its save memory (FieldTable.cpp kPlayerAccess, same order and sizes).
struct PlayerFieldDef {
    const char* name;
    uint16_t size; // bytes
};
inline constexpr PlayerFieldDef kPlayerFields[] = {
    { "rupees", 2 },        { "health", 2 },     { "magic", 1 },      { "swordHealth", 2 }, { "ammo", 24 },
    { "bottleItems", 6 },   { "buttonItems", 16 }, { "cButtonSlots", 16 }, { "dpad", 32 },     { "form", 1 },
    { "mask", 1 },          { "highScores", 28 }, { "dekuScores", 12 }, { "dekuNames", 24 },  { "picto", 8 },
    { "stolen", 4 },        { "scarecrow", 130 }, { "horse", 10 },     { "bombers", 6 },
};
inline constexpr size_t kPlayerFieldCount = sizeof(kPlayerFields) / sizeof(kPlayerFields[0]);
int FindPlayerField(const char* name); // index, or -1

// ---- The world's bytes ----
using FieldBytes = std::vector<std::vector<uint8_t>>; // one entry per coop::world::kFields, with its exact size
FieldBytes EmptyFields();                             // every field at zeros
uint32_t ReadU32(const uint8_t* at);                  // little-endian, as the game keeps them in memory
void WriteU32(uint8_t* at, uint32_t value);

// What a new co-op world starts with (SkipIntroSequence + SkipFirstCycle: Tatl, the Ocarina, Deku nuts, the Deku
// Mask, the Songs of Time and Healing, magic, one reset done and the intro's flags), added to `fields` where it is
// missing: nothing the world has is removed. A world that had no Ocarina is still in its first cycle (isFirstCycle).
// Returns the BaselinePart bits of what was missing (0 = nothing).
enum BaselinePart : uint32_t {
    BaselineTatl = 1u << 0,
    BaselineOcarina = 1u << 1,
    BaselineDekuNut = 1u << 2,
    BaselineDekuMask = 1u << 3,
    BaselineSongTime = 1u << 4,
    BaselineSongHealing = 1u << 5,
    BaselineMagic = 1u << 6,
    BaselineReset = 1u << 7,
    BaselineIntro = 1u << 8,
    BaselineFirstCycle = 1u << 9,
};
uint32_t ApplyCoopBaseline(FieldBytes& fields);

} // namespace coop::save

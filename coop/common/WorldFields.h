#pragma once
// Schema of the shared world (sub-project B): every field the server keeps, its size and how changes merge.
// The game maps each field to its save memory in mm/2s2h/Coop/World/FieldTable.cpp (checked at compile time).
// New field: add it at the END of kFields, map it in FieldTable.cpp and bump kProtocolVersion.
#include <cstddef>
#include <cstdint>

namespace coop::world {

enum class Kind : uint8_t {
    Bits,       // flags: merged per byte with (set, clear) masks, so players never undo each other's bits
    Bytes,      // values (item ids, levels, codes): the last change of each byte wins
    CounterU8,  // little-endian counters merged by adding deltas, clamped to the type's range
    CounterS8,
    CounterU16,
};

struct FieldDef {
    const char* name;
    uint16_t size; // bytes
    Kind kind;
};

inline constexpr FieldDef kFields[] = {
    { "weekEventReg", 100, Kind::Bits },      // quests and events (a few engine bits stay local, FieldTable.cpp)
    { "sceneFlags", 2400, Kind::Bits },       // cycleSceneFlags[120] {chest, switch0, switch1, clearedRoom, collectible}
    { "sceneRooms", 480, Kind::Bits },        // permanentSceneFlags[120].rooms (map rooms visited)
    { "owls", 2, Kind::Bits },                // owlActivationFlags
    { "mapsVisible", 28, Kind::Bits },        // scenesVisible[7] (Tingle's maps)
    { "regions", 4, Kind::Bits },             // regionsVisited
    { "clouds", 4, Kind::Bits },              // worldMapCloudVisibility
    { "gossipHearts", 4, Kind::Bits },        // unk_EA0 (Gossip Stones heart piece)
    { "blueWarps", 8, Kind::Bits },           // unk_EA8[2]
    { "upgrades", 4, Kind::Bits },            // inventory.upgrades (quiver, bomb bag, wallet...)
    { "quest", 4, Kind::Bits },               // inventory.questItems without heart pieces and the pictograph
    { "dungeonItems", 10, Kind::Bits },       // map, compass, boss key
    { "items", 18, Kind::Bytes },             // inventory.items[0..17] (not the bottles)
    { "masks", 24, Kind::Bytes },             // inventory.items[24..47]
    { "equipment", 2, Kind::Bytes },          // equips.equipment (sword and shield)
    { "magicFlags", 2, Kind::Bytes },         // isMagicAcquired, isDoubleMagicAcquired
    { "defense", 1, Kind::Bytes },            // doubleDefense
    { "progress", 3, Kind::Bytes },           // isFirstCycle, snowheadCleared, hasTatl
    { "resets", 2, Kind::Bytes },             // threeDayResetCount
    { "codes", 20, Kind::Bytes },             // lotteryCodes, spiderHouseMaskOrder, bomberCode (random, same for all)
    { "heartQuarters", 2, Kind::CounterU16 }, // virtual: max health in quarters (containers * 4 + pieces)
    { "bottles", 1, Kind::CounterU8 },        // virtual: bottles owned (their contents belong to each player)
    { "keys", 9, Kind::CounterS8 },           // inventory.dungeonKeys (-1 = none yet)
    { "fairies", 10, Kind::CounterU8 },       // inventory.strayFairies
    { "skulls", 4, Kind::CounterU16 },        // skullTokenCount: two counters (ocean, swamp)
    { "sceneExtra", 480, Kind::Bits },        // permanentSceneFlags[120].unk_14 (Great Fairy fountains, dungeon floors)
};

inline constexpr size_t kFieldCount = sizeof(kFields) / sizeof(kFields[0]);

int FindField(const char* name); // index, or -1
bool IsCounter(Kind kind);
int CounterWidth(Kind kind); // 1 or 2 bytes for counters, 0 otherwise
void CounterRange(Kind kind, int& min, int& max);

} // namespace coop::world

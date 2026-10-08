#pragma once
// [COOP] Drops compartidos (docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md): one item that fell in a
// game (rupees, hearts, magic, ammo, keys, fairies), as the games and the server see it: its network key, what it is and
// how it moves. A game announces it ("item"), the others create the same item from it, and the server keeps it for
// whoever arrives later ("items").
#include "Events.h"

#include <cstdint>

namespace coop {

// The actors that can be shared items (the game checks them against its own ids: Sync/DropRules.cpp).
constexpr int16_t kItemActorCollectible = 0x00E; // ACTOR_EN_ITEM00: rupees, hearts, magic, ammo, keys...
constexpr int16_t kItemActorFairy = 0x010;       // ACTOR_EN_ELF: a fairy that heals (from a pot, a drop, a song)
constexpr int16_t kItemActorStrayFairy = 0x1B0;  // ACTOR_EN_ELFORG: a stray fairy (from a pot, grass, a beehive)

// Keys: what one game dropped (its player and a counter), an item of a room's list (every game has it), or a twin
// (every game drops it at the same moment: the same key everywhere, so it exists once).
enum class ItemKeyKind : uint8_t { Twin, List, Unique };
constexpr uint32_t kUniqueItemKeyBit = 0x80000000u;
constexpr uint32_t kListItemKeyBit = 0x40000000u;

inline uint32_t MakeUniqueItemKey(uint8_t playerId, uint32_t seq) {
    return kUniqueItemKeyBit | ((uint32_t)(playerId & 0x3F) << 24) | (seq & 0xFFFFFFu);
}
inline uint32_t MakeListItemKey(int8_t room, int16_t index) {
    return kListItemKeyBit | ((uint32_t)(uint8_t)room << 16) | (uint16_t)index;
}
// The same in every game for the same spawner (its actor id and its room-list key) and slot (its number or counter).
uint32_t MakeTwinItemKey(int16_t spawnerId, uint32_t spawnerListKey, uint32_t slot);
inline ItemKeyKind ItemKeyKindOf(uint32_t key) {
    return (key & kUniqueItemKeyBit)  ? ItemKeyKind::Unique
           : (key & kListItemKeyBit) ? ItemKeyKind::List
                                     : ItemKeyKind::Twin;
}
inline uint8_t UniqueItemKeyPlayer(uint32_t key) {
    return (uint8_t)((key >> 24) & 0x3F);
}

struct ItemState {
    uint32_t key = 0;
    int16_t id = kItemActorCollectible;
    int32_t params = 0; // as it was created (0..0xFFFF)
    float pos[3] = { 0.f, 0.f, 0.f };
    // kItemActorCollectible only: how it moves (the same trajectory in every game)
    float vy = 0.f;     // velocity.y
    float speed = 0.f;
    float grav = 0.f;   // gravity
    float scale = 0.f;  // scale.x (a dropped item grows from 0)
    int16_t yaw = 0;    // world.rot.y: where it flies (and the line a falling heart sways along)
    int16_t phase = 0;  // home.rot.z: the sway of a falling heart
    int16_t timer = -1; // unk152: frames before it vanishes (< 0: never)
    uint8_t act = 0;    // 0 lying, 1 thrown out of what broke, 2 bouncing
};

// Writes key, id, params, pos and, for a collectible, the motion fields into an event.
void WriteItemState(const ItemState& s, json& out);
// False if something is missing, out of range or not a number.
bool ReadItemState(const json& ev, ItemState& out);
// How long the server keeps it lying (ms): a collectible's timer (20 frames a second) + a grace, a fairy 15 s;
// 0 = until someone takes it or the scene empties.
int64_t ItemLifeMs(const ItemState& s);

namespace item_limits {
constexpr float kMotion = 100.f;        // |vy|, |speed|, |grav|: far above what the game uses (8, 2, -0.9)
constexpr float kScale = 1.f;           // the biggest drop is 0.045
constexpr int kActions = 3;
constexpr int64_t kFairyLifeMs = 15000; // a drop fairy leaves after 240 frames
constexpr int64_t kLifeGraceMs = 3000;  // the timer waits while the item flies
} // namespace item_limits

} // namespace coop

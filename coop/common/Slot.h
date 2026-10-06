#pragma once
// [COOP] One 8-byte slot of memory as it travels (ActorImage.h: actors; EffectImage.h: effects; SoundEntry.h: where a
// sound plays). Pointers are translated so the receiver can find the same thing in its own process.
#include <cstdint>

namespace coop {

// How one 8-byte slot travels.
enum class SlotKind : uint8_t {
    Raw = 0,   // plain data: copied as it is
    Zero = 1,  // all zero (null pointers, cleared data)
    Keep = 2,  // a pointer that only means something in the sender's process: the receiver keeps its own
    Exe = 3,   // a pointer into the game's executable: value = offset from the image base
    Actor = 4, // a pointer into a replicated actor (or a list one: LocalListKey): key << 32 | region << 16 | offset
    Link = 5,  // a pointer into a Link (a player's own or a puppet): value = playerId << 32 | offset
    Scene = 6, // a pointer into the loaded scene's memory (paths, cutscene data): value = offset from sceneSegment
};
constexpr uint8_t kSlotKinds = 7;

struct Slot {
    SlotKind kind = SlotKind::Zero;
    uint64_t value = 0; // meaning depends on kind (Zero/Keep: 0)
    bool operator==(const Slot& o) const {
        return kind == o.kind && value == o.value;
    }
    bool operator!=(const Slot& o) const {
        return !(*this == o);
    }
};

inline uint64_t ActorRefValue(uint32_t key, uint8_t region, uint16_t offset) {
    return ((uint64_t)key << 32) | ((uint64_t)region << 16) | offset;
}

// The key of an Actor slot that points into an actor of the room's list nobody replicates (each game has its own: the
// balloon Bomber Jim shoots at): its list entry and its id + 1 in bits 16-29, so it is never a replicated actor's key
// (list keys: room << 8 | index; derived and runtime keys: bit 30 / 31). The receiver points at its own one.
constexpr uint16_t kMaxLocalListId = 0x3FFE;
inline uint32_t LocalListKey(uint16_t actorId, uint8_t room, uint8_t index) {
    return actorId > kMaxLocalListId ? 0 : ((uint32_t)(actorId + 1) << 16) | ((uint32_t)room << 8) | index;
}
inline bool ParseLocalListKey(uint32_t key, uint16_t& actorId, uint8_t& room, uint8_t& index) {
    if ((key & 0xC0000000u) != 0 || (key >> 16) == 0) {
        return false;
    }
    actorId = (uint16_t)((key >> 16) - 1);
    room = (uint8_t)(key >> 8);
    index = (uint8_t)key;
    return true;
}
inline uint64_t LinkRefValue(uint8_t playerId, uint32_t offset) {
    return ((uint64_t)playerId << 32) | offset;
}

} // namespace coop

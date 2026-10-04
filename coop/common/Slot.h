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
    Actor = 4, // a pointer into a replicated actor: value = key << 32 | region << 16 | offset
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
inline uint64_t LinkRefValue(uint8_t playerId, uint32_t offset) {
    return ((uint64_t)playerId << 32) | offset;
}

} // namespace coop

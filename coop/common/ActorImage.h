#pragma once
// [COOP] Sub-project D3: the full memory of the replicated actors of one room, sent by whoever simulates them (the
// room's authority, or the player an NPC is lent to). Binary stream kStreamActors on kChannelStream, one frame per
// game frame, cut into packets ("parts") of at most kPacketBytes that are applied independently. Layout:
// ActorImage.cpp. Everything here is plain data: the game decides what goes in (ActorSync.cpp).
//
// An actor is a list of memory regions (its instance, and tables of its skeletons/colliders kept outside it), seen as
// 8-byte slots. Each slot travels as one of the SlotKind below; a record carries the slots that changed recently.
#include "StreamIds.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

namespace image_limits {
constexpr size_t kPacketBytes = 1200;   // below the path MTU: ENet never fragments it
constexpr int kRegions = 16;            // instance + outside tables of one actor
constexpr uint32_t kRegionBytes = 0x10000; // no actor instance or table is bigger
constexpr int kSpanSlots = 255;         // slots in one run
constexpr int kSpans = 64;              // runs in one record
constexpr int kSfx = 8;
constexpr int kKeys = 255;              // alive/gone keys in one packet
constexpr int kRecords = 96;            // records in one packet
constexpr int kColliders = 8;
constexpr int8_t kRoomMax = 63;
constexpr float kWorldLimit = 32767.f;
} // namespace image_limits

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

// Consecutive slots of one region, starting at slot `first`.
struct SlotSpan {
    uint8_t region = 0;
    uint16_t first = 0; // slot index (byte offset / 8)
    std::vector<Slot> slots;
};

// How a runtime-created actor is created on the other games (sent in every full frame of it, so a late one gets it).
struct SpawnInfo {
    uint16_t actorId = 0;
    int16_t params = 0;
    float pos[3] = { 0.f, 0.f, 0.f };
    int16_t rot[3] = { 0, 0, 0 };
    uint32_t parentKey = 0; // 0: none
};

struct ActorImageRecord {
    uint32_t key = 0;
    uint16_t actorId = 0; // checked against the local actor with that key
    bool continuation = false; // more slots of the record before it (same key): masks, sounds and spawn are not here
    bool hasSpawn = false;
    SpawnInfo spawn;
    uint8_t acMask = 0;   // colliders the sender registered this frame (bit i = collider i)
    uint8_t ocMask = 0;
    std::vector<uint16_t> sfx; // sounds it started this frame
    std::vector<SlotSpan> spans;
};

struct ActorImagePacket {
    uint8_t playerId = 0; // stamped by the server when relaying (byte 1: StampPlayerId works)
    int16_t scene = -1;
    int8_t room = 0;
    uint16_t seq = 0;      // one per frame of this room: every part of a frame has the same
    uint8_t part = 0;
    uint8_t parts = 1;
    bool hasAlive = false; // `alive` is the complete list of keys the sender simulates in this room
    std::vector<uint32_t> alive;
    std::vector<uint32_t> gone; // keys destroyed recently
    std::vector<ActorImageRecord> records;
};

// One frame of a room into packets of at most kPacketBytes (part/parts filled in). A record that does not fit whole
// is split into several records with the same key (the extra ones carry only spans). Lists longer than a packet
// continue in the next one. Never more than 32 packets: the rest waits for the next frame.
std::vector<std::vector<uint8_t>> EncodeActorImage(const ActorImagePacket& frame);
// False unless the buffer is exactly one well-formed packet within every limit.
bool DecodeActorImage(const uint8_t* data, size_t size, ActorImagePacket& out);
// Only what the server needs to route it.
bool PeekActorImageHeader(const uint8_t* data, size_t size, int16_t& scene, int8_t& room);

// ---- Classifying a slot (the sender) ----

// What the sender's process knows about pointers (the game implements it; the tests fake it).
class PointerResolver {
  public:
    virtual ~PointerResolver() = default;
    virtual bool ExeOffset(uint64_t address, uint64_t& offset) const = 0;
    virtual bool ActorRef(uint64_t address, uint64_t& value) const = 0; // into a replicated actor's region
    virtual bool LinkRef(uint64_t address, uint64_t& value) const = 0;  // into a Link (ours or a puppet)
    virtual bool SceneOffset(uint64_t address, uint64_t& offset) const = 0; // into the loaded scene's memory
    virtual bool IsMapped(uint64_t address) const = 0; // any other memory of this process
};

// Zero, then pointers (exe, actor, Link, other mapped memory), else plain data.
Slot ClassifySlot(uint64_t raw, const PointerResolver& resolver);

} // namespace coop

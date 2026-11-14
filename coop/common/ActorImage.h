#pragma once
// [COOP] Sub-project D3: the full memory of the replicated actors of one room, sent by whoever simulates them (the
// room's authority, or the player an NPC is lent to). Binary stream kStreamActors on kChannelStream, one frame per
// game frame, cut into packets ("parts") of at most kPacketBytes that are applied independently. Layout:
// ActorImage.cpp. Everything here is plain data: the game decides what goes in (ActorSync.cpp).
//
// An actor is a list of memory regions (its instance, and tables of its skeletons/colliders kept outside it), seen as
// 8-byte slots. Each slot travels as one of the SlotKind below; a record carries the slots that changed recently.
#include "Slot.h"
#include "SoundEntry.h"
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
constexpr int kSfx = sound_limits::kPerRecord;
constexpr int kKeys = 255;              // alive/gone keys in one packet
constexpr int kRecords = 96;            // records in one packet
constexpr int kColliders = 8;
constexpr int8_t kRoomMax = 63;
constexpr int8_t kPlayerRoom = 63; // the stream of each player's own objects (arrows, bombs...): not a room of a scene
constexpr float kWorldLimit = 32767.f;
} // namespace image_limits

// Consecutive slots of one region, starting at slot `first`.
struct SlotSpan {
    uint8_t region = 0;
    uint16_t first = 0; // slot index (byte offset / 8)
    std::vector<Slot> slots;
};

// "No parent" in a key field. Never 0: 0 is a real key, the first actor of room 0's list (Gyorg in its lair: with 0 as
// "none" the copies of its small fish were created without their parent, set its static pointer to null, and the next
// fish that went away crashed the game).
constexpr uint32_t kNoParentKey = 0xFFFFFFFFu;

// How a runtime-created actor is created on the other games (sent in every full frame of it, so a late one gets it).
struct SpawnInfo {
    uint16_t actorId = 0;
    int16_t params = 0;
    float pos[3] = { 0.f, 0.f, 0.f };
    int16_t rot[3] = { 0, 0, 0 };
    uint32_t parentKey = kNoParentKey;
};

// A changed slot goes out in this many records that are really sent (a lost packet costs nothing).
constexpr uint8_t kSlotResends = 3;

// Whether this frame's record of an actor carries one of its slots. `left` = records that must still carry its last
// change; it only counts down on frames whose record goes out (`sending`): far from every Link a record goes out once
// every 4 frames, and counting frames instead let a change made between two sends never travel until the next full
// frame (up to a second). Fields that change together but sit in different slots (an enemy's colorFilterParams and
// colorFilterTimer, a Wizrobe's platforms and their count) then reached the copy apart: a timer with no duration was
// a division by zero when it was drawn.
inline bool SlotResend_Carry(uint8_t& left, bool changed, bool full, bool sending) {
    if (changed) {
        left = kSlotResends;
    }
    bool carry = full || left > 0;
    if (carry && sending && left > 0) {
        left--;
    }
    return carry;
}

struct ActorImageRecord {
    uint32_t key = 0;
    uint16_t actorId = 0; // checked against the local actor with that key
    bool continuation = false; // more slots of the record before it (same key): masks, sounds and spawn are not here
    bool hasSpawn = false;
    SpawnInfo spawn;
    uint8_t acMask = 0;   // colliders the sender registered this frame (bit i = collider i)
    uint8_t ocMask = 0;
    std::vector<SoundEntry> sfx; // what it sounded this frame
    int16_t dyna = -1;           // its dynamic collision's flags (the BGACTOR_* that travel); -1: not in this record
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
    // Bookkeeping each game's engine keeps for itself, even inside the exe (the light nodes of sLightsBuffer): a pointer
    // into it means nothing in another game (the same index there is someone else's light).
    virtual bool EngineOwned(uint64_t address) const {
        return false;
    }
};

// Zero, then what the engine keeps for itself (Keep), then pointers (exe, actor, Link, other mapped memory), else
// plain data.
Slot ClassifySlot(uint64_t raw, const PointerResolver& resolver);
// The receiver: true = this slot keeps what it has now (it holds one of our engine's own nodes, or what arrived would
// put one there). A copy's Destroy removes the light its slot names: it must be the one its own Init added.
bool KeepsOwnSlot(uint64_t current, uint64_t incoming, const PointerResolver& resolver);

} // namespace coop

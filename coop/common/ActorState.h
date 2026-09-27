#pragma once
// Binary stream of the shared enemies of one room (channel kChannelStream), sent by the room's authority once per
// game frame (sub-project C). A frame of a big room can take several packets ("parts"). Layout: ActorState.cpp.
#include "PlayerState.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

constexpr uint8_t kStreamActors = 2;

namespace actor_limits {
constexpr size_t kPacketBytes = 1200; // stays below the path MTU: ENet never has to fragment it
constexpr int kJoints = 32;           // the enemies of the list have 8-28 limbs (+1 root)
constexpr int kColliders = 4;
constexpr int kSfx = 4;
constexpr int kExtras = 16;
constexpr int kGone = 16;
constexpr int kParts = 16;
constexpr int kActorsPerFrame = 64;
constexpr int8_t kRoomMax = 63;
constexpr float kScaleLimit = 100.f;
} // namespace actor_limits

struct ActorCollider {
    static constexpr uint8_t kAc = 1; // can be hit (its AC is registered on the replica)
    static constexpr uint8_t kOc = 2; // pushes (OC registered)
    uint8_t flags = 0;
    Vec3s16 dimPos; // cylinders: dim.pos (other shapes place themselves while drawing)
};

struct ActorRecord {
    uint16_t key = 0;     // the actor's index in the room's actor list (0x8000 | id: created at runtime, C2)
    uint16_t actorId = 0; // checked against the local actor with that key
    int16_t params = 0;
    bool visible = false; // drawn by its owner this frame
    float pos[3] = { 0.f, 0.f, 0.f };
    Vec3s16 rot; // shape.rot
    int16_t worldRotY = 0;
    float scale[3] = { 0.f, 0.f, 0.f };
    uint8_t health = 0;
    uint16_t colorFilterParams = 0;
    uint8_t colorFilterTimer = 0;
    uint8_t shadowAlpha = 0;
    std::vector<Vec3s16> joints; // skelAnime.jointTable
    std::vector<ActorCollider> colliders;
    std::vector<uint16_t> sfx;    // sounds it played this frame
    std::vector<uint8_t> extras;  // per-enemy draw state (SharedActors.cpp)
};

struct ActorPacket {
    uint8_t playerId = 0; // stamped by the server when relaying (byte 1, like poses: StampPlayerId works)
    int16_t scene = -1;
    int8_t room = 0;
    uint16_t seq = 0;   // one per frame of this room (every part of a frame has the same)
    uint8_t part = 0;   // 0..parts-1
    uint8_t parts = 1;
    uint16_t total = 0; // records in all the parts of this frame (set by the encoder)
    std::vector<uint16_t> gone; // keys that died or went away recently (part 0 only)
    std::vector<ActorRecord> actors;
};

// Splits frame.actors (and frame.gone) into packets of at most kPacketBytes; part/parts/total are filled in.
// Lists longer than their limit are cut, and at most kActorsPerFrame records / kParts packets go out.
std::vector<std::vector<uint8_t>> EncodeActorPackets(const ActorPacket& frame);
// False when the buffer is not exactly one well-formed, plausible packet.
bool DecodeActorPacket(const uint8_t* data, size_t size, ActorPacket& out);
// Only the header (what the server needs to route it). False if too short, another type or an impossible room.
bool PeekActorHeader(const uint8_t* data, size_t size, int16_t& scene, int8_t& room);

} // namespace coop

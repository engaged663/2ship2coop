#pragma once
// [COOP] Sub-project D3: the replicated actors of the loaded scene. Each one has a network key, the memory regions
// that travel (its instance and tables it keeps outside it), a copy of those regions right after its Init (what a
// "keep" slot goes back to) and the sync state of ActorSync.cpp / HitSync.cpp.
#include "ReplicationRules.h"

#include "common/ActorImage.h"

#include <cstdint>
#include <utility>
#include <vector>

extern "C" {
#include "z64.h"
}

namespace coop::client {

// Keys: actors of the room's list (room << 8 | index), children created in a replicated actor's Init (both games
// create them: derived from the parent), and actors created at run time by the one that simulates their parent.
constexpr uint32_t kDerivedKeyBit = 0x40000000u;
constexpr uint32_t kRuntimeKeyBit = 0x80000000u;
inline uint32_t ListKey(int8_t room, int16_t index) {
    return ((uint32_t)(uint8_t)room << 8) | (uint32_t)(index & 0xFF);
}
inline bool IsListKey(uint32_t key) {
    return (key & (kDerivedKeyBit | kRuntimeKeyBit)) == 0;
}

// A hit another player's game saw on its copy of this actor ("hit" event).
struct PendingHit {
    uint8_t from = 0; // attacker's player id
    uint8_t col = 0;
    uint8_t elem = 0;
    uint32_t dmgFlags = 0;
    uint8_t effect = 0;
    uint8_t damage = 0;
    uint8_t hitEffect = 0;
    int16_t pos[3] = {};
    bool oc = false;         // a contact (OC), not an attack: a guest's Link or explosive touched it
    uint16_t attackerId = 0; // what touched it: ACTOR_PLAYER, ACTOR_EN_BOM, ACTOR_EN_BOM_CHU
};

struct Region {
    uint8_t* ptr = nullptr;
    uint32_t size = 0;
    std::vector<uint8_t> init; // its bytes right after Init
    std::vector<bool> rawOnly; // per slot: never a pointer (joint tables)
};

struct TrackedActor {
    Actor* actor = nullptr;
    uint32_t key = 0;
    uint32_t rootKey = 0;   // its list ancestor: a lent NPC takes its whole family along
    uint32_t parentKey = 0; // 0: none
    int8_t room = -1;       // the room it belongs to (its list room, or its parent's)
    bool runtime = false;   // created at run time by the game that simulates its parent
    bool cinema = false;    // Replication::Cinema: its owner is Cinema_ActorOwner() (Leases.cpp)
    SpawnInfo spawn;        // runtime ones: how the other games create it
    uint16_t initChildren = 0; // children created in its Init so far (their derived keys)
    std::vector<Region> regions;      // [0] = instance
    std::vector<Collider*> colliders; // in creation order (the same in every game)
    std::vector<SkelAnime*> skels;    // in creation order
    uint8_t fullPhase = 0;            // which frame of every 20 its complete state goes out
    std::vector<uint8_t> localMask; // region 0, one byte per byte: 1 = this game's own (never sent nor written): the
                                    // base Actor's engine fields and, for a DynaPolyActor, its bgId and interactFlags
    std::vector<std::pair<uint8_t*, uint32_t>> extraRegions; // tables its Init asked to travel (Coop_AddActorRegion)

    // Sender (we simulate it)
    std::vector<std::vector<Slot>> sentSlots;        // per region, per slot: the last classified value
    std::vector<std::vector<uint32_t>> changedFrame; // per region, per slot: frame of its last change
    uint8_t acMask = 0; // colliders registered this frame (CollisionCheck_SetAC / SetOC)
    uint8_t ocMask = 0;
    uint8_t sentAc = 0; // the masks last sent (a change to "none" must be sent too)
    uint8_t sentOc = 0;
    std::vector<uint16_t> oneShotSfx;    // played during this frame's update
    Actor* target = nullptr;             // the Link it chased last frame
    std::vector<PendingHit> pendingHits; // hits other players made on it, applied before its next update
    uint8_t lastHitFrom = 0;             // the remote player whose hit we applied last (HitSync_InjectPending)...
    int64_t lastHitMs = 0;               // ...and when: a kill is theirs if it came soon after (Mods/GameEvents.cpp)

    // Receiver (a copy)
    std::vector<SlotSpan> pendingSpans; // received, written before its next (skipped) update
    std::vector<uint16_t> pendingSfx;
    uint8_t remoteAc = 0;
    uint8_t remoteOc = 0;
    bool hasState = false; // the owner's state arrived at least once
    int64_t lastSeenMs = 0;
    uint8_t missedAlive = 0; // alive lists of its owner that did not name it
    uint8_t hitCooldown[image_limits::kColliders] = {};
    uint8_t ocCooldown[image_limits::kColliders] = {};
    const Actor* ocLast[image_limits::kColliders] = {}; // the explosive whose contact we reported last (once each)
};

TrackedActor* ActorRegistry_Get(const Actor* actor); // nullptr: not replicated
TrackedActor* ActorRegistry_Find(uint32_t key);
std::vector<TrackedActor*> ActorRegistry_All();
void ActorRegistry_Clear();
uint32_t ActorRegistry_Version(); // changes whenever an actor is added or removed

// A runtime actor another game announced is a cutscene actor (itself, or made by one): only who watches that game's
// cutscene creates its copy.
bool ActorRegistry_IsCinemaSpawn(uint32_t parentKey, const SpawnInfo& s);
// ActorSync.cpp creates the copy of a runtime actor another game announced: its Init registers it under this key.
void ActorRegistry_ExpectReplica(uint32_t key, uint32_t parentKey, uint32_t rootKey, int8_t room, const SpawnInfo& s);
void ActorRegistry_EndExpect(); // after that Actor_Spawn (it may have failed before creating anything)
// A local actor nobody else gets (never tracked, never echoed): the stand-ins of HitSync.cpp's contacts, the actors of
// the server's mods (Mods/GameOps.cpp).
Actor* ActorRegistry_SpawnUntracked(int16_t id, const Vec3f& pos, s16 params, s16 rotY = 0);

} // namespace coop::client

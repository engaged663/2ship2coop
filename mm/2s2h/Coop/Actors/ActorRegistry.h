#pragma once
// The shared enemies of the loaded scene (sub-project C): which actor has which network key, and the skeleton and
// colliders it created in its Init (captured through the engine hooks of CoopEngine.h).
#include "SharedActors.h"

#include "common/ActorState.h"

#include <cstdint>
#include <vector>

extern "C" {
#include "z64.h"
}

namespace coop::client {

// A hit another player's game saw on its replica of this enemy ("hit" event).
struct PendingHit {
    uint8_t from = 0; // attacker's player id
    uint8_t col = 0;
    uint8_t elem = 0;
    uint32_t dmgFlags = 0;
    uint8_t effect = 0;
    uint8_t damage = 0;
    uint8_t hitEffect = 0;
    int16_t pos[3] = {};
};

struct TrackedActor {
    Actor* actor = nullptr;
    uint16_t key = 0; // its index in the room's actor list
    int8_t room = -1;
    SkelAnime* skel = nullptr;         // the skeleton it draws (first one initialized in its Init)
    std::vector<Collider*> colliders;  // in creation order (the same in every game)
    const SharedActorDef* def = nullptr;

    // Sync state (ActorSync.cpp / HitSync.cpp)
    coop::ActorRecord record;       // the owner's latest state (replicas)
    bool hasRecord = false;
    bool drawn = true;              // the owner draws it
    int64_t lastSeenMs = 0;         // when the owner last listed it
    Actor* target = nullptr;        // the Link it chased last frame (owner)
    std::vector<uint16_t> oneShotSfx; // played during this frame's update (owner)
    uint8_t hitCooldown[4] = {};    // frames until each collider may report a hit again (replicas)
    std::vector<PendingHit> pendingHits; // hits other players made on it, applied before its next update (owner)
};

TrackedActor* ActorRegistry_Get(const Actor* actor);         // nullptr: not a shared enemy
TrackedActor* ActorRegistry_Find(int8_t room, uint16_t key);
std::vector<TrackedActor*> ActorRegistry_All();
void ActorRegistry_Clear();

} // namespace coop::client

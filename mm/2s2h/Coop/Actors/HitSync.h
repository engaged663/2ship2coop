#pragma once
// Hits between players and shared enemies (spec §5-§6). Our attacks on a replica are sent to the room's owner
// ("hit"), which applies them to its enemy with the engine's own damage rules; the owner's enemies hitting a
// puppet are sent to that player ("hurt"), whose game applies them to its Link the same way.
#include "ActorRegistry.h"

struct Actor;
struct PlayState;

namespace coop::client {

void HitSync_ReportReplicaHits(TrackedActor& replica); // before a replica shows its new state (ActorSync.cpp)
void HitSync_InjectPending(TrackedActor& owned);       // before an enemy of ours updates (ActorSync.cpp)
void HitSync_ClearMarks(TrackedActor& replica);        // after its new state is written: no stale hit flags
// The puppet's update (PuppetActor.cpp): reads what hit it last frame and registers its body for this one.
void HitSync_PuppetUpdate(Actor* puppet, uint8_t playerId, PlayState* play);

} // namespace coop::client

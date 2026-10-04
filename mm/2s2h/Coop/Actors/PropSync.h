#pragma once
// [COOP] Shared props (PropSync.cpp): pots, grass, crates, barrels, rocks and rupees lying around are broken, cut or
// picked up for everyone in the scene, and what they drop is seen by everyone (the first to pick it up gets it).
#include <cstdint>

struct Actor;

namespace coop::client {

// ActorRegistry.cpp (Coop_OnActorSpawned): an actor was just created; an item a prop drops becomes shared.
void PropSync_OnSpawned(Actor* actor);
// One of kSharedProps: PropSync.cpp removes it for everyone (Sync/FlagReload.cpp never creates it again).
bool PropSync_IsShared(int16_t actorId);

} // namespace coop::client

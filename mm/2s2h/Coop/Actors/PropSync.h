#pragma once
// [COOP] Shared props (PropSync.cpp): pots, grass, crates, barrels, rocks and rupees lying around are broken, cut or
// picked up for everyone in the scene, and what they drop is seen by everyone (the first to pick it up gets it).
struct Actor;

namespace coop::client {

// ActorRegistry.cpp (Coop_OnActorSpawned): an actor was just created; an item a prop drops becomes shared.
void PropSync_OnSpawned(Actor* actor);

} // namespace coop::client

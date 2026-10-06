#pragma once
// [COOP] Sub-project D3: NPCs lent to the player next to them ("leases" from the server). Whoever holds the lease of
// an actor's family (its root key) simulates it; otherwise the owner of its room (Authority.h) does.
#include "ActorRegistry.h"

namespace coop::client {

uint8_t Leases_Holder(int8_t room, uint32_t rootKey); // 0: not lent
// Who simulates this actor now: its lessee, or its room's owner (0: nobody known).
uint8_t Leases_Owner(const TrackedActor& t);
bool Leases_IsMine(const TrackedActor& t);
bool Leases_IsRemote(const TrackedActor& t);
bool Leases_HoldAny();                  // we simulate some lent NPC
bool Leases_LentToMe(const TrackedActor& t); // lent to us (not ours as the room's owner)
void Leases_Tick();                     // once per frame: ask for the NPCs next to us, give back the far ones
// A cutscene of a shared actor we simulate is running here (or just ended): only its own update ends it, so its
// family stays ours meanwhile (held as if we talked to it), and our rooms too (Features/Location.cpp: not busy).
bool Leases_HoldsCutscene();

} // namespace coop::client

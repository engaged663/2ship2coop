#pragma once
// [COOP] Sub-project D3: which actors are the same for every game (simulated by one, copied to the others), which
// every game creates for itself, and which are lent to the player next to them. The tables are in
// ReplicationRules.cpp: add an actor id there to change how it is treated.
#include <cstdint>

extern "C" {
#include "z64.h"
}

namespace coop::client {

enum class Replication : uint8_t {
    Local,      // each game has its own (as without co-op)
    Replicated, // one game simulates it, the others show a copy of its memory
    Echo,       // created at run time by a replicated actor: every game creates its own copy
    Cinema,     // a cutscene actor: ours, except while we watch another game's shared cutscene (that game drives it)
};

// An actor of the room's actor list (profile category, id).
Replication Rules_ListActor(int16_t actorId, uint8_t category);
// An actor created while a replicated actor of this game updates.
Replication Rules_RuntimeChild(int16_t actorId, uint8_t category);
// Lent to the player next to it (NPCs: talking, shops, minigames).
bool Rules_Leasable(int16_t actorId, uint8_t category);
// A prop of the room's list that goes with an NPC there (kPartners: Bomber Jim's balloon): that NPC's id, -1 if none.
// ActorRegistry.cpp puts it in the NPC's family.
int16_t Rules_PartnerNpc(int16_t propId);
// One of the cutscene actors (Dm_*, Demo_*), whatever the option says; and the option (gCoop.Group.CinemaActors:
// off = each game's own, never followed, as before the limits fix).
bool Rules_IsCutsceneActor(int16_t actorId);
bool Rules_CinemaActorsOn();

} // namespace coop::client

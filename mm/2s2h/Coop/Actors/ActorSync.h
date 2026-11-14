#pragma once
// [COOP] Sub-project D3: whoever simulates a replicated actor (the owner of its room, or the player it is lent to)
// sends its memory every frame; the other games show a copy that never runs its own logic (ActorSync.cpp).
#include "ActorRegistry.h"

struct Actor;
struct Player;
struct PlayState;

namespace coop::client {

void ActorSync_FrameEnd();                        // CoopInit.cpp: sends what we simulate
void ActorSync_OnDestroyed(TrackedActor& actor);  // ActorRegistry.cpp: remembered as gone for the other games
void ActorSync_ForgetPointersTo(const Actor* actor); // ActorRegistry.cpp: copies stop pointing at a destroyed actor
// A live replicated actor (ours or a copy) keeps a pointer into this actor (FlagReload.cpp waits before recreating it:
// the graveyard's Stalchildren keep their gravestone while they open it)
bool ActorSync_PointedAt(const Actor* actor);
TrackedActor* ActorSync_Updating();               // the replicated actor of ours whose update runs (nullptr if none)
Actor* ActorSync_AnyUpdating();                   // any actor whose update runs now (nullptr between updates)

} // namespace coop::client

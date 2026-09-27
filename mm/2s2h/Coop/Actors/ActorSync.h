#pragma once
// Shared enemies (sub-project C): the room's owner sends their state every frame; the other games turn them into
// replicas that copy it instead of running their own logic. The owner's enemies chase the nearest Link.
#include "ActorRegistry.h"

struct Actor;
struct Player;
struct PlayState;

namespace coop::client {

void ActorSync_FrameEnd();                      // CoopInit.cpp: sends the frame of every room we own
void ActorSync_OnDestroyed(TrackedActor& actor); // ActorRegistry.cpp: remembered as gone for the other games
TrackedActor* ActorSync_Updating();              // the enemy of ours whose update is running (nullptr if none)

} // namespace coop::client

#pragma once
// Grabs (spec docs/superpowers/specs/2026-10-26-coop-agarres-design.md): an enemy we simulate that grabs another
// player's puppet grabs that player's own Link ("grab"); what it does to the puppet while it holds it is done to that
// Link in its game. The engine's side: Coop_OnPlayerGrabbed, Coop_GrabVictimText (CoopEngine.h).
#include <cstdint>

struct Actor;
struct Player;

namespace coop::client {

// ActorSync.cpp, around the update of an actor of ours: compares the puppet it holds before and after.
void GrabSync_UpdateBegin(Actor* actor);
void GrabSync_UpdateEnd(Actor* actor);
// The Link an enemy of ours holds (a puppet, or our own Link): it keeps chasing that one while it holds it.
Player* GrabSync_Prey(Actor* actor);
void GrabSync_OnActorGone(const Actor* actor); // ActorSync.cpp: a grabber of ours was destroyed
// PuppetActor.cpp: state bits the puppet keeps on top of its pose's (held, until its player's pose says so).
uint32_t GrabSync_PuppetFlags2(uint8_t playerId, uint32_t poseFlags2);

} // namespace coop::client

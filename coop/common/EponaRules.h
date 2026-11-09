#pragma once
// [COOP] Epona with several players: the decisions of the game's Epona code (mm/2s2h/Coop/Features/CoopEponaManager.cpp)
// as plain rules, tested in coop/tests/TestEponaState.cpp.

#include <cstdint>

namespace coop {

// A horse offers itself to the local Link as he comes close (EnHorse_MountDismount -> Actor_SetRideActor): never while
// he rides, or is getting on, another one. Riding into another player's Epona used to move the rider onto it (both
// players on one horse) and left his own horse "ridden" by nobody: he could not get off properly.
bool EponaRules_MayMount(const void* currentRide, const void* horse);

// What Epona's Song does where horses may come.
enum class EponaSong : uint8_t {
    CallOurs, // our horse is here already: it comes to us (the game's own reaction), never a second one
    Wake,     // the scene's resting Epona wakes up and becomes ours
    Spawn,    // none at all: a new one next to us
};
EponaSong EponaRules_OnSong(bool ownHorseHere, bool restingHorseHere);

} // namespace coop

#pragma once
// Keeps one En_CoopPuppet actor per remote player that is in our scene, fed by the pose stream.
#include "common/PlayerState.h"

#include <cstdint>

struct Actor;
struct PlayState;

// Once per game frame, while the local player updates: advances the jitter buffers and
// spawns/kills puppets as players enter/leave our scene.
void PuppetManager_Update(PlayState* play);

// The pose the puppet of playerId must show this frame. False if none.
bool PuppetManager_GetCurrent(uint8_t playerId, coop::PlayerState& out);

void PuppetManager_OnPuppetDestroyed(uint8_t playerId, Actor* actor);
// The puppet actor of playerId in our scene, or nullptr (sub-project C: enemies chase and hurt puppets).
Actor* PuppetManager_Actor(uint8_t playerId);
void PuppetManager_OnSceneEnd(); // every actor is going away with the scene
void PuppetManager_RemovePlayer(uint8_t playerId);
void PuppetManager_Clear();

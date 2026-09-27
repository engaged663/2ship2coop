#ifndef COOP_ENGINE_H
#define COOP_ENGINE_H

// [COOP] What the engine's C code calls for the shared enemies (sub-project C). Every function does nothing outside
// the server's shared world, so the game behaves exactly as before there.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"

// GET_PLAYER(play) returns this while it is non-NULL: set only around the update of a shared enemy of this game,
// so it chases the nearest Link (ours or another player's puppet).
extern struct Player* gCoopPlayerOverride;

// Around actor->init (Actor_Init and the deferred init of Actor_UpdateActor): the skeleton and colliders created
// in between belong to that actor.
void Coop_ActorInitBegin(Actor* actor);
void Coop_ActorInitEnd(Actor* actor);
void Coop_OnSkelAnimeInit(SkelAnime* skelAnime);
void Coop_OnColliderSet(Collider* collider);

// Before a (not frozen) actor updates, in Actor_UpdateActor. Returns the Link it must use for its distances this
// frame (NULL = the local one, as always) and sets *forceUpdate when it must update even outside the camera.
Player* Coop_ActorUpdateBegin(PlayState* play, Actor* actor, s32* forceUpdate);
void Coop_ActorUpdateEnd(PlayState* play, Actor* actor);

// func_800B8D10 (knockback of GET_PLAYER). 1 = it was aimed at a puppet: sent to its player instead.
s32 Coop_OnKnockback(PlayState* play, Actor* actor, f32 speed, s16 yaw, f32 velY, s32 type, u32 damage);
// Actor_PlaySfx: a shared enemy's sounds also play on the other games.
void Coop_OnActorSfx(Actor* actor, u16 sfxId);
// Item_DropCollectibleRandom (fn 0) / Item_DropCollectible (fn 1) while a shared enemy of this game updates:
// every other game in the scene drops its own.
void Coop_OnDrop(PlayState* play, Vec3f* pos, s32 params, s32 fn);

#ifdef __cplusplus
}
#endif

#endif // COOP_ENGINE_H

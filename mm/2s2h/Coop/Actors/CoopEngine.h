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
// D3: an actor was created (Actor_SpawnAsChildAndCutscene, its fields set, before its Init): who created it decides
// whether it is replicated, and with which key.
void Coop_OnActorSpawned(Actor* actor);
// D3: a collider went into this frame's collision pass (CollisionCheck_SetAC / SetOC; ac = 1 for AC, 0 for OC).
void Coop_OnColliderRegistered(Collider* collider, s32 ac);

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
// 1 = the actor updating now must drop nothing (grass cut because another player cut it; PropSync.cpp).
s32 Coop_DropSuppressed(void);

// Time never stops in the server's world (TimeNeverStops.cpp). Coop_InWorld: playing in it.
s32 Coop_InWorld(void);
// The player's state flags without the ones that freeze the world (talking, item pick-ups, ocarina...).
u32 Coop_FreezeStateFlags(u32 stateFlags1);
// While the pause menu is open the world runs, but Link (and the horse he rides) waits: 1 = do not update this actor.
s32 Coop_HoldWhilePaused(PlayState* play, Actor* actor);

// The end of the game together (Features/Ending*.cpp). Coop_InEnding: this game plays the ending (everything of the
// co-op stands aside, as the original). Coop_CutsceneHold: 1 = this frame the ending's cutscene waits for a player
// who is behind (z_demo.c). Coop_OnFinale: the ending reached its last shot (the cutscene "runs forever").
s32 Coop_InEnding(void);
s32 Coop_CutsceneHold(PlayState* play);
void Coop_OnFinale(PlayState* play);

// Groups (Activities/Rewards.cpp, Features/Cinema.cpp). Coop_OnGetItem: the local Link gets this GetItem now (before
// its Item_Give; giver = the actor that offered it); Coop_OnGetItemEnd: after it. Coop_OnRupeesChanged: every
// Rupees_ChangeBy. Coop_OnBossTitleCard: TitleCard_InitBossName. Coop_CsActionTarget: the Link a cutscene action of
// Player_SetCsAction* goes to (ours when our camera shows the cutscene of a shared actor chasing a puppet).
void Coop_OnGetItem(PlayState* play, s32 getItemId, Actor* giver);
void Coop_OnGetItemEnd(void);
void Coop_OnRupeesChanged(s16 rupeeChange);
void Coop_OnBossTitleCard(PlayState* play, TexturePtr texture, s16 x, s16 y, u8 width, u8 height);
struct Player* Coop_CsActionTarget(PlayState* play, Actor* csActor);

// Limits fix (spec 2026-09-30-coop-grupos-limites §2.2): from a replicated actor's Init, a table of its own outside
// its instance (static in its overlay) travels with its memory.
void Coop_AddActorRegion(Actor* actor, void* table, u32 bytes);

// The values a mirrored text shows (Features/MessageVars.cpp): begin = 1 before Message_Decode, 0 after it.
void Coop_OnMessageDecode(PlayState* play, s32 begin);

// Effects echo (Features/EffectEcho.cpp). Coop_OnEffectSpawn: every EffectSs_Spawn, before it runs.
// Coop_CollisionPass: begin = 1 before this frame's collision checks (z_play.c), 0 after them: the hit marks made in
// between come from our Link's and our actors' attacks.
void Coop_OnEffectSpawn(PlayState* play, s32 type, s32 priority, void* initData);
void Coop_CollisionPass(s32 begin);

#ifdef __cplusplus
}
#endif

#endif // COOP_ENGINE_H

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
// Sincronización total S1 (Sync/SoundEcho.cpp). Coop_OnSfx: every AudioSfx_PlaySfx, before it runs; 1 = it must not
// sound here (a copy's own code: its owner sends its sounds). Coop_OnWorldSfx: SoundSource_Add, begin = 1 before and
// 0 after. Coop_FlaggedAudio: around Actor_UpdateFlaggedAudio (those travel with the actor's memory).
s32 Coop_OnSfx(u16 sfxId, Vec3f* pos, u8 token, f32* freqScale, f32* volume, s8* reverbAdd);
void Coop_OnWorldSfx(PlayState* play, Vec3f* worldPos, u32 duration, u16 sfxId, u32 eachFrame, s32 begin);
void Coop_FlaggedAudio(s32 begin);
// Item_DropCollectibleRandom (fn 0) / Item_DropCollectible (fn 1) while a shared enemy of this game updates:
// every other game in the scene drops its own.
void Coop_OnDrop(PlayState* play, Vec3f* pos, s32 params, s32 fn);
// 1 = the actor updating now must drop nothing (grass cut because another player cut it; PropSync.cpp).
s32 Coop_DropSuppressed(void);

// Time never stops in the server's world (TimeNeverStops.cpp). Coop_InWorld: playing in it.
s32 Coop_InWorld(void);
// The player's state flags without the ones that freeze the world (talking, item pick-ups, ocarina...).
u32 Coop_FreezeStateFlags(u32 stateFlags1);
// While a pause screen that is not the live menu (below) is open the world runs, but Link (and the horse he rides)
// waits: 1 = do not update this actor. On the owls' map of the Song of Soaring he waits too.
s32 Coop_HoldWhilePaused(PlayState* play, Actor* actor);

// The pause menu of the server's world stops nothing (LiveMenu.cpp). Coop_PauseLive: that menu is open (the owls' map
// of the Song of Soaring counts; the game over screens do not): the world, Link included, runs and is drawn behind it.
s32 Coop_PauseLive(PlayState* play);
// For the engine's "is the game paused?" checks that stop something of the world: paused, and not behind that menu.
#define COOP_WORLD_PAUSED(play) (IS_PAUSED(&(play)->pauseCtx) && !Coop_PauseLive(play))
// 1 = Link waits even behind the live menu (Coop_HoldWhilePaused asks).
s32 Coop_LiveMenuHoldsLink(PlayState* play);
// Play_UpdateMain, once a frame before the world runs: Coop_PauseLive, and keeps track of the menu opening and closing.
s32 Coop_LiveMenuBegin(PlayState* play);
// Around what runs behind the menu (the world, the camera). neutral = 1: controller 1 becomes one nobody touches;
// 0: the real one is back.
void Coop_LiveMenuInput(PlayState* play, s32 neutral);
// After the world ran: the menu closes by itself (no animation) if the game took Link or the screen. Returns what it
// did; something open over the menu (the Bombers' Notebook) is the caller's to close.
#define COOP_MENU_CLOSE_NO 0
#define COOP_MENU_CLOSE_DROP_TEXT 1 // closing; a text box still open was the menu's own and went with it
#define COOP_MENU_CLOSE_KEEP_TEXT 2 // closing; the open text box is the game's
s32 Coop_LiveMenuAfterWorld(PlayState* play);
// The menu's update of this frame, instead of KaleidoScopeCall_Update: the game keeps its 20 frames a second with
// the menu open, the menu keeps its own 30 updates a second.
void Coop_LiveMenuUpdate(PlayState* play);
// After the update of something with text boxes of its own open over the menu (the Bombers' Notebook).
void Coop_LiveMenuOwnText(PlayState* play);
// The co-op itself needs the screen (a new world arrived: a cycle reset, the moon): that menu closes now.
void Coop_LiveMenuClose(PlayState* play);

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

// Mods of the server (Mods/GameEvents.cpp): Enemy_StartFinishingBlow, a killing blow landed on this enemy here (only
// the game that simulates it runs that code). The mods hear who gave it.
void Coop_OnEnemyDefeated(PlayState* play, Actor* actor);

// Effects echo (Features/EffectEcho.cpp). Coop_OnEffectSpawn: every EffectSs_Spawn, before it runs; 1 = do not create
// it here (the Init of a copy we create: its owner sends it). Coop_CollisionPass: begin = 1 before this frame's
// collision checks (z_play.c), 0 after them: the hit marks made in between come from our Link's and our actors' attacks.
s32 Coop_OnEffectSpawn(PlayState* play, s32 type, s32 priority, void* initData);
void Coop_CollisionPass(s32 begin);

// Sincronización total (Sync/CopyCode.cpp): around a Draw and a Destroy (a copy's own code runs silent, with its
// owner's Link as GET_PLAYER). Coop_BlockAT: 1 = this collider must not attack in this game (copies and puppets).
void Coop_ActorDrawBegin(Actor* actor);
void Coop_ActorDrawEnd(Actor* actor);
void Coop_ActorDestroyBegin(Actor* actor);
void Coop_ActorDestroyEnd(Actor* actor);
s32 Coop_BlockAT(Collider* collider);

// Sincronización total S2 (Sync/AmbientEcho.cpp): music orders and quakes of what this game simulates. The music
// functions call Coop_MusicBegin(kind, value) first and Coop_MusicEnd() last (kinds of common/Ambient.h); the raw
// orders inside them are not counted again. Coop_QuakeRead: a request's values (type, speed, y, x, fov, roll,
// duration); 0 if there is none.
void Coop_OnSeqCmd(u32 cmd);
void Coop_MusicBegin(s32 kind, u32 value);
void Coop_MusicEnd(void);
void Coop_OnQuake(s16 index);
s32 Coop_QuakeRead(s16 index, s16* out);

// Sincronización total S4 (Sync/FlagReload.cpp): an actor reads a scene flag (kind 0 switch, 1 chest, 2 clear,
// 3 clear temp, 4 collectible). gCoopSpawnRoom >= 0: the room of the actor Actor_SpawnAsChildAndCutscene creates now
// (FlagReload.cpp creates an actor again in its own room).
void Coop_OnFlagRead(s32 kind, s32 flag);
extern s32 gCoopSpawnRoom;

// Sincronización total S5 (Sync/SceneObjects.cpp): an actor stands on a dynamic collision actor this frame.
void Coop_OnCarried(DynaPolyActor* dyna, Actor* carried);

// Sincronización total S6 (Sync/OcarinaEcho.cpp): the note our controller plays (instrument 0: not playing), whether
// our own ocarina is busy, and another player's note played on the ocarina channel while ours is quiet.
void Coop_OcarinaRead(u8* instrument, u8* pitch, f32* bend, s8* vibrato);
s32 Coop_OcarinaLocalBusy(void);
void Coop_OcarinaRemote(u8 instrument, u8 pitch, f32 bend, s8 vibrato, Vec3f* pos);
void Coop_OcarinaRemoteStop(void);

#ifdef __cplusplus
}
#endif

#endif // COOP_ENGINE_H

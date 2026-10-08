#pragma once
// [COOP] Sincronización total: sounds, music and quakes, the players' objects, the scene's flags and machinery, the
// ocarina (spec docs/superpowers/specs/2026-10-04-coop-sincronizacion-total-design.md). Map of this folder:
//   SyncOptions.cpp   the switches: gCoop.Sync.* (1 by default) AND what the server allows (welcome "sync")
//   SfxIds.cpp        whether a sound id exists in this game (the banks' tables), for the echo and the mods
//   SoundEcho.cpp     S1: the sounds of our Link and of what we simulate travel; the others' play on their puppet/copy
//   CopyCode.cpp      a copy's own code (Init, Draw, Destroy): silent, its owner's Link as GET_PLAYER, no attacks
//   AmbientEcho.cpp   S2: music orders and quakes of what we simulate ("ambient")
//   PlayerObjects.cpp S3: our arrows, bombs, hookshot... (kPlayerObjects: ADD A LINE) and what we carry
//   SceneFlags.cpp    S4: the loaded scene's flags, live ("sflag", "sflags")
//   FlagReload.cpp    S4: what read a flag only when it was created is created again when another player changes it
//   SceneObjects.cpp  S5: the machinery that is the same for everyone (kSceneObjects: ADD A LINE); who touches it runs it
//   OcarinaEcho.cpp   S6: the notes of the others' ocarina, and the songs they play right ("song")
//   DropRules.cpp     Drops: which created actors are shared items, and the twins' spawners (kTwinSpawners: ADD A LINE)
//   SharedDrops.cpp   Drops: what falls is one item for everyone; the first one to take it gets it
//                     (spec docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md)
//   SyncMenu.cpp      the menu section and the list of what is synced nearby
// The puppets' sword trail lives in Puppet/PuppetActor.cpp.
#include "common/SoundEntry.h"

#include <cstdint>
#include <vector>

extern "C" {
#include "z64.h"
}

namespace coop::client {

struct TrackedActor;

enum class SyncPart : uint8_t { Sounds, Ambient, PlayerObjects, SceneFlags, SceneObjects, Ocarina, Drops, Count };

// SyncOptions.cpp
bool Sync_On(SyncPart part);           // playing in the server's world, its CVar on and the server allows it
bool Sync_ServerAllows(SyncPart part); // the server's "sync" (true when it says nothing)
const char* Sync_CVar(SyncPart part);  // "gCoop.Sync.Sounds"...
const char* Sync_Name(SyncPart part);  // for the menu (Spanish)

// SfxIds.cpp
bool Sfx_Valid(uint16_t sfxId); // a sound of this game's tables

// SoundEcho.cpp
std::vector<SoundEntry> SoundEcho_TakeLinkSounds();          // this frame's sounds of our Link (PoseCapture.cpp)
void SoundEcho_PlayOn(Actor* body, const SoundEntry& sound); // a sound another game sent, on its puppet or copy here
void SoundEcho_SilenceBegin();                               // a copy's own code runs: its sounds are its owner's
void SoundEcho_SilenceEnd();

// CopyCode.cpp
void CopyCode_Begin(Actor* actor, bool init); // around an Init, a Draw or a Destroy (always paired with End)
void CopyCode_End();
bool CopyCode_InCopyInit();                                     // the Init of a copy we are creating runs now
bool CopyCode_Running();                                        // a copy's own code (Init, Draw, Destroy) runs now
void CopyCode_NoteDyingCopy(const Actor* actor, uint8_t owner); // ActorRegistry.cpp: untracked before its Destroy

// PlayerObjects.cpp
bool PlayerObjects_Listed(int16_t actorId);
bool PlayerObjects_MayCreate(uint32_t key, uint32_t parentKey, uint8_t sender); // a copy of the sender's object
TrackedActor* PlayerObjects_AdoptFor(uint32_t key, uint16_t actorId, uint8_t sender); // what another one carries

// SceneFlags.cpp
// FieldTable.cpp wrote the world's cycle flags into the loaded scene (before: what the scene had)
void SceneFlags_NoteWorldWrite(const ActorContextSceneFlags& before);

// FlagReload.cpp
void FlagReload_InitBegin(Actor* actor);
void FlagReload_InitEnd();
void FlagReload_OnRemoteChange(int kind, int flag); // kind: 0 switch, 1 chest, 2 clear, 3 clear temp, 4 collectible

// SceneObjects.cpp
enum class TouchPolicy : uint8_t {
    None,  // its room's owner runs it (hits on it travel with "hit")
    Touch, // who stands on it (or leaves a bomb, a statue on it) runs it while there
    Near,  // who comes within reach (push blocks) runs it
};
bool SceneObjects_Shared(int16_t actorId);      // kSceneObjects or gCoop.Sync.Shared, and the part is on
bool SceneObjects_ForcedLocal(int16_t actorId); // gCoop.Sync.Local: never replicated, whatever its category
TouchPolicy SceneObjects_Policy(int16_t actorId);
bool SceneObjects_Touched(const Actor* actor, float distToLink); // our Link or one of our actors touches it now
const char* SceneObjects_ActorName(int16_t actorId);             // GameIds.inc name ("OBJ_RAILLIFT"), "?" if none

// DropRules.cpp (Drops compartidos): what SharedDrops_OnSpawned does with an actor just created (before its Init);
// spawner = the actor whose update runs now (nullptr: none).
enum class DropKind : uint8_t {
    None,     // not a shared item
    ListItem, // an item of the room's list: every game has it, only who takes it is shared
    Dropped,  // it fell here: announced, the others create the same item
};
DropKind DropRules_Classify(const Actor* actor, const Actor* spawner);
bool DropRules_IsTwinSpawner(int16_t actorId);
// The key of an item a twin spawner made (the same in every game), 0 if it is not one of its own.
uint32_t DropRules_TwinKey(const Actor* item, Actor* spawner, uint32_t spawnerSeq);

// SharedDrops.cpp
bool SharedDrops_OnSpawned(Actor* actor);    // ActorRegistry.cpp: true = a shared item (never replicated nor echoed)
bool SharedDrops_Tracks(const Actor* actor); // one of our shared items (LiveFlags.cpp leaves it to us)
struct SharedDropRow {
    uint32_t key;
    int16_t actorId;
    bool mine;     // it fell in our game
    uint8_t claim; // 0 nobody asked for it here, 1 we asked the server, 2 ours
    float dist;
};
std::vector<SharedDropRow> SharedDrops_Nearby(float maxDist); // the menu's list, nearest first

// SyncMenu.cpp
void SyncMenu_Draw();

} // namespace coop::client

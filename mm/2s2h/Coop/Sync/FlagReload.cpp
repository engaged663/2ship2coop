// [COOP] Sincronización total S4 (spec §5.3): many objects read their flag only in their Init (a wall broken by a bomb,
// a rock, a block that appears) and never react when another player changes it. Here we note which flags each actor
// reads in its Init and which it looks at again in its update; when another player changes one, every local actor of
// the room's list that read it only when created is created again (same list index, room, cutscene and half days):
// its new Init sees the new state. Never replicated ones, enemies, Link, the puppets, doors, what our Link holds,
// talks to, rides or stands on, an actor in its cutscene, nor kNeverReload. gCoop.Sync.FlagReload (and SceneFlags).
// Also the world's events (weekEventReg, World/WorldSync.cpp): the Woodfall temple another player raised with the
// Sonata. A list actor that killed itself in its Init because of what it read (the temple's ramp and its Deku flower,
// "not risen yet") is brought back the same way; so is a cutscene actor of ours (Dm_*) while no other game drives it.
// A change that finds its actor busy (in a cutscene, stood on) is tried again for a while.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Actors/PropSync.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <initializer_list>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

extern "C" s32 gCoopSpawnRoom = -1;

namespace coop::client {

namespace {

constexpr uint32_t kRetryFrames = 20 * 30; // a change waits this long (30 s) for its busy actors
constexpr size_t kMaxReads = 24;           // flags noted per actor and per kind of read
constexpr size_t kMaxPending = 256;
constexpr size_t kMaxDormant = 128;

// Never created again: what LiveFlags.cpp removes, what polls its flag anyway, what opens rooms or warps.
bool NeverReload(int16_t id) {
    for (int16_t n : { ACTOR_EN_ITEM00, ACTOR_ITEM_B_HEART, ACTOR_EN_ELFORG, ACTOR_EN_SI, ACTOR_EN_BOX,
                       ACTOR_OBJ_WARPSTONE, ACTOR_EN_HOLL, ACTOR_DOOR_WARP1, ACTOR_EN_COOP_PUPPET }) {
        if (n == id) {
            return true;
        }
    }
    return false;
}

// kind << 16 | flag (kinds: Sync.h, FlagReload_OnRemoteChange)
using Key = uint32_t;
Key Pack(int kind, int flag) {
    return ((uint32_t)kind << 16) | ((uint32_t)flag & 0xFFFF);
}

struct Reads {
    std::vector<Key> init;   // read in its Init
    std::vector<Key> polled; // read in its update
    uint32_t initFrame = 0;  // when its Init ran (a change older than it is already seen)
};

struct InitFrame {
    Actor* actor = nullptr;
    bool spawned = false; // it created something in its Init
};

// A list actor that killed itself in its Init after reading these flags: created again when one changes.
struct Dormant {
    s16 id = 0;
    Vec3f pos = {};
    Vec3s rot = {};
    s32 params = 0;
    u32 cs = 0x7F;
    u32 halfDays = 0;
    s8 room = -1;
    s16 index = -1;
    uint32_t initFrame = 0;
    std::vector<Key> reads;
};

struct Change {
    Key key = 0;
    uint32_t frame = 0; // when it arrived
};

bool sOn = false;
uint32_t sFrame = 1;
std::vector<InitFrame> sInit;
std::unordered_map<const Actor*, Reads> sReads;
std::vector<Dormant> sDormant;
std::vector<Change> sChanged; // changes of other players not yet served

bool Has(const std::vector<Key>& v, Key k) {
    return std::find(v.begin(), v.end(), k) != v.end();
}

void Note(std::vector<Key>& v, Key k) {
    if (v.size() < kMaxReads && !Has(v, k)) {
        v.push_back(k);
    }
}

Player* LocalLink() {
    return (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool RoomLoaded(s8 room) {
    return room == gPlayState->roomCtx.curRoom.num || room == gPlayState->roomCtx.prevRoom.num;
}

// Created again only if it is a local actor, or a cutscene actor of ours no other game drives now.
bool ReloadableTracking(const Actor* a) {
    const TrackedActor* t = ActorRegistry_Get(a);
    return t == nullptr || (t->cinema && !Leases_IsRemote(*t));
}

bool EligibleCategory(const Actor* a) {
    switch (a->category) {
        case ACTORCAT_PLAYER:
        case ACTORCAT_ENEMY:
        case ACTORCAT_BOSS:
        case ACTORCAT_EXPLOSIVES:
        case ACTORCAT_DOOR:
            return false;
        default:
            return !NeverReload(a->id) && !PropSync_IsShared(a->id);
    }
}

bool Eligible(const Actor* a) {
    if (a->update == nullptr || a->init != nullptr || GetActorListIndex(a) < 0 || !ReloadableTracking(a) ||
        !EligibleCategory(a)) {
        return false;
    }
    Player* link = LocalLink();
    if (link != nullptr && (link->heldActor == a || link->talkActor == a || link->rideActor == a ||
                            link->interactRangeActor == a || link->csActor == a || link->doorActor == a)) {
        return false;
    }
    if (link != nullptr && link->actor.floorBgId != BGCHECK_SCENE &&
        (const Actor*)DynaPoly_GetActor(&gPlayState->colCtx, link->actor.floorBgId) == a) {
        return false;
    }
    return !(a->csId != CS_ID_NONE && CutsceneManager_GetCurrentCsId() == a->csId);
}

void KillChildren(const Actor* parent) {
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->parent == parent && a->update != nullptr && ActorRegistry_Get(a) == nullptr) {
                Actor_Kill(a);
            }
        }
    }
}

// The list entry as it was created (Actor_SpawnEntry), in its own room.
void Spawn(s16 id, const Vec3f& pos, const Vec3s& rot, s32 params, u32 cs, u32 halfDays, s8 room, s16 index) {
    currentActorListIndex = index;
    gCoopSpawnRoom = room;
    Actor_SpawnAsChildAndCutscene(&gPlayState->actorCtx, gPlayState, id, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z,
                                  params, cs, halfDays, nullptr);
    gCoopSpawnRoom = -1;
    currentActorListIndex = -1;
}

void Reload(Actor* a) {
    s16 id = a->id;
    Vec3f pos = a->home.pos;
    Vec3s rot = a->home.rot;
    s32 params = a->params;
    u32 cs = a->csId == CS_ID_NONE ? 0x7F : (u32)a->csId;
    u32 halfDays = (u32)(u16)a->halfDaysBits;
    s8 room = a->room;
    s16 index = GetActorListIndex(a);
    SPDLOG_INFO("[Coop] Another player changed a flag actor {:#x} read when created: created again", (uint16_t)id);
    KillChildren(a);
    ActorRegistry_Untrack(a); // a cutscene actor of ours: the new one takes its key
    Actor_Kill(a);
    Spawn(id, pos, rot, params, cs, halfDays, room, index);
}

// A live actor of that list entry (it came back by itself: never two).
bool EntryAlive(const Dormant& d) {
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->update != nullptr && a->id == d.id && a->room == d.room && GetActorListIndex(a) == d.index) {
                return true;
            }
        }
    }
    return false;
}

// A change it has not seen: newer than its Init, and it read that flag only when created.
bool Hit(const std::vector<Key>& init, const std::vector<Key>* polled, uint32_t initFrame) {
    for (const Change& c : sChanged) {
        if (c.frame > initFrame && Has(init, c.key) && (polled == nullptr || !Has(*polled, c.key))) {
            return true;
        }
    }
    return false;
}

void FrameEnd() {
    sOn = Sync_On(SyncPart::SceneFlags) && CVarGetInteger("gCoop.Sync.FlagReload", 1) != 0 && gPlayState != nullptr;
    sFrame++;
    if (!sOn) {
        sChanged.clear();
        sDormant.clear();
        return;
    }
    // What another room had is gone with it (its actors are made again when it loads)
    sDormant.erase(std::remove_if(sDormant.begin(), sDormant.end(), [](const Dormant& d) { return !RoomLoaded(d.room); }),
                   sDormant.end());
    sChanged.erase(std::remove_if(sChanged.begin(), sChanged.end(),
                                  [](const Change& c) { return sFrame - c.frame > kRetryFrames; }),
                   sChanged.end());
    if (sChanged.empty() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF ||
        gPlayState->transitionMode != TRANS_MODE_OFF) {
        return;
    }
    std::vector<Actor*> reload;
    for (const auto& [actor, reads] : sReads) {
        Actor* a = (Actor*)actor;
        if (Hit(reads.init, &reads.polled, reads.initFrame) && Eligible(a)) {
            reload.push_back(a);
        }
    }
    for (Actor* a : reload) {
        if (a->update != nullptr) { // not killed meanwhile with another one's children
            Reload(a);
        }
    }
    std::vector<Dormant> wake;
    for (auto it = sDormant.begin(); it != sDormant.end();) {
        if (Hit(it->reads, nullptr, it->initFrame)) {
            wake.push_back(std::move(*it));
            it = sDormant.erase(it);
        } else {
            ++it;
        }
    }
    for (const Dormant& d : wake) {
        if (!EntryAlive(d)) {
            SPDLOG_INFO("[Coop] Another player changed a flag actor {:#x} read before leaving: created again",
                        (uint16_t)d.id);
            Spawn(d.id, d.pos, d.rot, d.params, d.cs, d.halfDays, d.room, d.index); // its Init says if it stays
        }
    }
}

void Forget() {
    sReads.clear();
    sChanged.clear();
    sInit.clear();
    sDormant.clear();
}

void RegisterFlagReload() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) { sReads.erase(actor); });
    COND_HOOK(OnPlayDestroy, true, Forget);
}

// A list actor that just killed itself in its Init after reading flags, without creating anything (what made
// something before leaving turned into it: never brought back).
void NoteDormant(Actor* a, const InitFrame& frame) {
    auto it = sReads.find(a);
    int16_t index = GetActorListIndex(a);
    if (frame.spawned || it == sReads.end() || it->second.init.empty() || index < 0 || a->room < 0 ||
        !ReloadableTracking(a) || !EligibleCategory(a) || sDormant.size() >= kMaxDormant) {
        return;
    }
    Dormant d;
    d.id = a->id;
    d.pos = a->home.pos;
    d.rot = a->home.rot;
    d.params = a->params;
    d.cs = a->csId == CS_ID_NONE ? 0x7F : (u32)a->csId;
    d.halfDays = (u32)(u16)a->halfDaysBits;
    d.room = a->room;
    d.index = index;
    d.initFrame = it->second.initFrame;
    d.reads = it->second.init;
    sDormant.erase(std::remove_if(sDormant.begin(), sDormant.end(),
                                  [&](const Dormant& o) { return o.room == d.room && o.index == d.index; }),
                   sDormant.end());
    sDormant.push_back(std::move(d));
}

// One read: in an Init, or in the update of an actor that read something in its Init (the others never matter).
void OnRead(Key k) {
    if (!sInit.empty()) {
        Actor* a = sInit.back().actor;
        Reads& r = sReads[a];
        r.initFrame = sFrame;
        Note(r.init, k);
        return;
    }
    Actor* a = ActorSync_AnyUpdating();
    auto it = a != nullptr ? sReads.find(a) : sReads.end();
    if (it != sReads.end()) {
        Note(it->second.polled, k);
    }
}

} // namespace

void FlagReload_InitBegin(Actor* actor) {
    sInit.push_back({ actor, false });
}

void FlagReload_InitEnd(Actor* actor) {
    if (sInit.empty()) {
        return;
    }
    InitFrame frame = sInit.back();
    sInit.pop_back();
    if (sOn && frame.actor == actor && actor->update == nullptr && gPlayState != nullptr) {
        NoteDormant(actor, frame);
    }
}

void FlagReload_NoteSpawn() {
    if (!sInit.empty()) {
        sInit.back().spawned = true;
    }
}

void FlagReload_OnRemoteChange(int kind, int flag) {
    Key k = Pack(kind, flag);
    for (Change& c : sChanged) {
        if (c.key == k) {
            c.frame = sFrame; // changed again: newer
            return;
        }
    }
    if (sChanged.size() < kMaxPending) {
        sChanged.push_back({ k, sFrame });
    }
}

static RegisterShipInitFunc sFlagReloadInit(RegisterFlagReload);

} // namespace coop::client

using namespace coop::client;

extern "C" void Coop_OnFlagRead(s32 kind, s32 flag) {
    if (!sOn || flag < 0 || flag >= 128) {
        return;
    }
    OnRead(Pack(kind, flag));
}

// Every CHECK_WEEKEVENTREG (z64save.h): flag = byte << 8 | bits.
extern "C" void Coop_OnWeekEventRead(s32 flag) {
    if (!sOn || (sInit.empty() && ActorSync_AnyUpdating() == nullptr)) {
        return;
    }
    int byte = (flag >> 8) & 0xFF;
    for (int bit = 1; bit < 0x100; bit <<= 1) {
        if (flag & bit) {
            OnRead(Pack(kFlagWorldEvent, (byte << 8) | bit));
        }
    }
}

// [COOP] Sincronización total S4 (spec §5.3): many objects read their flag only in their Init (a wall broken by a bomb,
// a rock, a block that appears) and never react when another player changes it. Here we note which flags each actor
// reads in its Init and which it looks at again in its update; when another player changes one, every local actor of
// the room's list that read it only when created is created again (same list index, room, cutscene and half days):
// its new Init sees the new state. Never replicated ones, enemies, Link, the puppets, doors, what our Link holds,
// talks to, rides or stands on, an actor in its cutscene, nor kNeverReload. gCoop.Sync.FlagReload (and SceneFlags).
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
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

struct Reads {
    std::vector<uint16_t> init;   // read in its Init
    std::vector<uint16_t> polled; // read in its update
};

bool sOn = false;
std::vector<Actor*> sInit;
std::unordered_map<const Actor*, Reads> sReads;
std::vector<uint16_t> sChanged; // changes of other players, handled at the end of the frame

uint16_t Pack(int kind, int flag) {
    return (uint16_t)(kind * 128 + flag);
}

bool Has(const std::vector<uint16_t>& v, uint16_t k) {
    return std::find(v.begin(), v.end(), k) != v.end();
}

Player* LocalLink() {
    return (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool Eligible(const Actor* a) {
    if (a->update == nullptr || a->init != nullptr || GetActorListIndex(a) < 0 || ActorRegistry_Get(a) != nullptr ||
        NeverReload(a->id) || PropSync_IsShared(a->id)) {
        return false;
    }
    switch (a->category) {
        case ACTORCAT_PLAYER:
        case ACTORCAT_ENEMY:
        case ACTORCAT_BOSS:
        case ACTORCAT_EXPLOSIVES:
        case ACTORCAT_DOOR:
            return false;
        default:
            break;
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

void Reload(Actor* a) {
    PlayState* play = gPlayState;
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
    Actor_Kill(a);
    currentActorListIndex = index;
    gCoopSpawnRoom = room;
    Actor_SpawnAsChildAndCutscene(&play->actorCtx, play, id, pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, params, cs,
                                  halfDays, nullptr);
    gCoopSpawnRoom = -1;
    currentActorListIndex = -1;
}

void FrameEnd() {
    sOn = Sync_On(SyncPart::SceneFlags) && CVarGetInteger("gCoop.Sync.FlagReload", 1) != 0 && gPlayState != nullptr;
    if (sChanged.empty()) {
        return;
    }
    std::vector<uint16_t> changed;
    changed.swap(sChanged);
    if (!sOn) {
        return;
    }
    std::vector<Actor*> reload;
    for (const auto& [actor, reads] : sReads) {
        Actor* a = (Actor*)actor;
        bool hit = std::any_of(changed.begin(), changed.end(),
                               [&](uint16_t k) { return Has(reads.init, k) && !Has(reads.polled, k); });
        if (hit && Eligible(a)) {
            reload.push_back(a);
        }
    }
    for (Actor* a : reload) {
        if (a->update != nullptr) { // not killed meanwhile with another one's children
            Reload(a);
        }
    }
}

void Forget() {
    sReads.clear();
    sChanged.clear();
    sInit.clear();
}

void RegisterFlagReload() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) { sReads.erase(actor); });
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

void FlagReload_InitBegin(Actor* actor) {
    sInit.push_back(actor);
}

void FlagReload_InitEnd() {
    if (!sInit.empty()) {
        sInit.pop_back();
    }
}

void FlagReload_OnRemoteChange(int kind, int flag) {
    if (sChanged.size() < 256) {
        sChanged.push_back(Pack(kind, flag));
    }
}

static RegisterShipInitFunc sFlagReloadInit(RegisterFlagReload);

} // namespace coop::client

using namespace coop::client;

extern "C" void Coop_OnFlagRead(s32 kind, s32 flag) {
    if (!sOn || flag < 0 || flag >= 128) {
        return;
    }
    bool init = !sInit.empty();
    Actor* a = init ? sInit.back() : ActorSync_AnyUpdating();
    if (a == nullptr) {
        return;
    }
    Reads& r = sReads[a];
    std::vector<uint16_t>& v = init ? r.init : r.polled;
    uint16_t k = Pack(kind, flag);
    if (v.size() < 24 && !Has(v, k)) {
        v.push_back(k);
    }
}

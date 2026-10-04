// [COOP] Sub-project D3: tracks the replicated actors as they are created (ActorRegistry.h). The engine tells us
// when an actor is created (Coop_OnActorSpawned: who made it), when its Init starts and ends, and which skeletons
// and colliders it sets up in between (CoopEngine.h). Only pieces inside the actor's own memory are kept.
#include "ActorRegistry.h"

#include "ActorMemory.h"
#include "ActorSync.h"
#include "Authority.h"
#include "CoopEngine.h"
#include "Leases.h"
#include "PropSync.h"

#include "2s2h/Coop/Sync/Sync.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <memory>
#include <unordered_map>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

// Decided when the actor is created, used when its Init runs (right away, or later if its object was not loaded).
struct SpawnCtx {
    bool tracked = false;
    uint32_t key = 0;
    uint32_t parentKey = 0;
    uint32_t rootKey = 0;
    int8_t room = -1;
    bool runtime = false;
    SpawnInfo spawn;
    bool cinema = false;
    uint8_t owner = 0; // a player's own object (Sync/PlayerObjects.cpp)
};

std::unordered_map<const Actor*, std::unique_ptr<TrackedActor>> sTracked;
std::unordered_map<uint32_t, TrackedActor*> sByKey;
std::unordered_map<const Collider*, TrackedActor*> sColliderOwner;
std::unordered_map<const Actor*, SpawnCtx> sSpawnCtx;
std::vector<TrackedActor*> sInitializing; // innermost last; nullptr: not replicated
uint32_t sVersion = 0;
uint32_t sRuntimeSeq = 0;
bool sExpecting = false;
SpawnCtx sExpected;
bool sEchoing = false; // creating an echo another game sent: never tracked, never sent back

TrackedActor* Current() {
    return sInitializing.empty() ? nullptr : sInitializing.back();
}

uint32_t InstanceSize(const Actor* actor) {
    return (actor->overlayEntry != nullptr && actor->overlayEntry->profile != nullptr)
               ? actor->overlayEntry->profile->instanceSize
               : 0;
}

bool Inside(const Actor* actor, const void* p, size_t size) {
    uintptr_t start = (uintptr_t)actor;
    uintptr_t end = start + InstanceSize(actor);
    return (uintptr_t)p >= start && (uintptr_t)p + size <= end;
}

// A child created in a replicated actor's Init: the same in every game (same parent, same order).
uint32_t DerivedKey(uint32_t parent, uint16_t index) {
    uint32_t h = 2166136261u;
    for (uint32_t v : { parent, (uint32_t)index }) {
        for (int i = 0; i < 4; i++) {
            h = (h ^ ((v >> (8 * i)) & 0xFF)) * 16777619u;
        }
    }
    return kDerivedKeyBit | (h & 0x3FFFFFFFu);
}

uint8_t Category(int16_t id) {
    if (id < 0 || id >= ACTOR_ID_MAX || gActorOverlayTable[id].profile == nullptr) {
        return ACTORCAT_MAX;
    }
    return gActorOverlayTable[id].profile->type;
}

// Slots completely inside a joint table are never pointers (a pointer sharing a slot with its first bytes still is).
void MarkRawOnly(TrackedActor& t, const void* p, size_t bytes) {
    for (Region& r : t.regions) {
        uintptr_t start = (uintptr_t)r.ptr;
        uintptr_t a = (uintptr_t)p;
        if (p == nullptr || a < start || a + bytes > start + r.size) {
            continue;
        }
        size_t first = (a - start + 7) / 8;
        size_t last = (a + bytes == start + r.size) ? r.rawOnly.size() : (a - start + bytes) / 8;
        for (size_t s = first; s < last && s < r.rawOnly.size(); s++) {
            r.rawOnly[s] = true;
        }
        return;
    }
}

// A table the actor keeps outside its instance (allocated by its Init) travels as its own region.
void AddRegion(TrackedActor& t, void* p, size_t bytes) {
    if (p == nullptr || bytes == 0 || bytes > image_limits::kRegionBytes ||
        t.regions.size() >= (size_t)image_limits::kRegions || Inside(t.actor, p, bytes)) {
        return;
    }
    for (const Region& r : t.regions) {
        if (r.ptr == p) {
            return; // two skeletons sharing a table
        }
    }
    Region r;
    r.ptr = (uint8_t*)p;
    r.size = (uint32_t)bytes;
    t.regions.push_back(std::move(r));
}

void BuildRegions(TrackedActor& t) {
    Region instance;
    instance.ptr = (uint8_t*)t.actor;
    instance.size = std::min<uint32_t>(InstanceSize(t.actor), image_limits::kRegionBytes);
    t.regions.push_back(std::move(instance));
    for (SkelAnime* skel : t.skels) {
        size_t bytes = (size_t)skel->limbCount * sizeof(Vec3s);
        AddRegion(t, skel->jointTable, bytes);
        AddRegion(t, skel->morphTable, bytes);
    }
    for (Collider* col : t.colliders) {
        if (col->shape == COLSHAPE_JNTSPH) {
            ColliderJntSph* j = (ColliderJntSph*)col;
            AddRegion(t, j->elements, (size_t)std::max(j->count, 0) * sizeof(ColliderJntSphElement));
        } else if (col->shape == COLSHAPE_TRIS) {
            ColliderTris* tr = (ColliderTris*)col;
            AddRegion(t, tr->elements, (size_t)std::max(tr->count, 0) * sizeof(ColliderTrisElement));
        }
    }
    for (const auto& [ptr, bytes] : t.extraRegions) {
        AddRegion(t, ptr, bytes);
    }
    for (Region& r : t.regions) {
        r.rawOnly.assign((r.size + 7) / 8, false);
    }
    for (SkelAnime* skel : t.skels) {
        size_t bytes = (size_t)skel->limbCount * sizeof(Vec3s);
        MarkRawOnly(t, skel->jointTable, bytes);
        MarkRawOnly(t, skel->morphTable, bytes);
    }
    for (Region& r : t.regions) {
        r.init.assign(r.ptr, r.ptr + r.size);
    }
    t.sentSlots.assign(t.regions.size(), {});
    t.changedFrame.assign(t.regions.size(), {});
    t.fullPhase = (uint8_t)((t.key * 2654435761u) % 20);
    ActorMemory_BuildLocalMask(t);
}

void Track(Actor* actor, const SpawnCtx& ctx) {
    if (sByKey.count(ctx.key) != 0) {
        SPDLOG_WARN("[Coop] Key {:#x} already used (actor {:#x}): not replicated", ctx.key, (uint16_t)actor->id);
        return;
    }
    auto t = std::make_unique<TrackedActor>();
    t->actor = actor;
    t->key = ctx.key;
    t->parentKey = ctx.parentKey;
    t->rootKey = ctx.rootKey != 0 ? ctx.rootKey : ctx.key;
    t->room = ctx.room;
    t->runtime = ctx.runtime;
    t->cinema = ctx.cinema;
    t->owner = ctx.owner;
    t->spawn = ctx.spawn;
    sInitializing.back() = t.get();
    sByKey[ctx.key] = t.get();
    sTracked[actor] = std::move(t);
    sVersion++;
}

void Untrack(const Actor* actor) {
    auto it = sTracked.find(actor);
    if (it == sTracked.end()) {
        return;
    }
    for (Collider* c : it->second->colliders) {
        sColliderOwner.erase(c);
    }
    sByKey.erase(it->second->key);
    sTracked.erase(it);
    sVersion++;
}

void SendEcho(Actor* actor, int8_t room) {
    json ev = MakeEvent(ev::kEcho);
    ev["scene"] = gPlayState->sceneId;
    ev["room"] = room;
    ev["id"] = actor->id;
    ev["params"] = actor->params;
    ev["pos"] = { actor->home.pos.x, actor->home.pos.y, actor->home.pos.z };
    ev["rot"] = { actor->home.rot.x, actor->home.rot.y, actor->home.rot.z };
    NetClient::Get().SendEvent(ev);
}

// Another game's replicated actor created one of these: we create our own copy.
void OnEcho(const json& ev) {
    if (!WorldSession_Active() || EndingMode_Active() || gPlayState == nullptr ||
        GetInt(ev, "scene", -1) != gPlayState->sceneId) {
        return;
    }
    int16_t id = (int16_t)GetInt(ev, "id", -1);
    if (Rules_RuntimeChild(id, Category(id)) != Replication::Echo) {
        return; // only what the rules say every game creates for itself
    }
    auto pos = ev.find("pos");
    auto rot = ev.find("rot");
    if (pos == ev.end() || rot == ev.end() || !pos->is_array() || !rot->is_array() || pos->size() != 3 ||
        rot->size() != 3) {
        return;
    }
    f32 p[3];
    s16 r[3];
    for (int i = 0; i < 3; i++) {
        double v = (*pos)[i].is_number() ? (*pos)[i].get<double>() : NAN;
        double w = (*rot)[i].is_number() ? (*rot)[i].get<double>() : 0.0;
        if (!std::isfinite(v) || !std::isfinite(w)) {
            return;
        }
        p[i] = (f32)v;
        r[i] = (s16)(int32_t)std::clamp(w, -32768.0, 65535.0);
    }
    sEchoing = true;
    Actor_Spawn(&gPlayState->actorCtx, gPlayState, id, p[0], p[1], p[2], r[0], r[1], r[2], (s32)GetInt(ev, "params"));
    sEchoing = false;
}

} // namespace

TrackedActor* ActorRegistry_Get(const Actor* actor) {
    auto it = sTracked.find(actor);
    return it == sTracked.end() ? nullptr : it->second.get();
}

TrackedActor* ActorRegistry_Find(uint32_t key) {
    auto it = sByKey.find(key);
    return it == sByKey.end() ? nullptr : it->second;
}

std::vector<TrackedActor*> ActorRegistry_All() {
    std::vector<TrackedActor*> out;
    out.reserve(sTracked.size());
    for (auto& [actor, tracked] : sTracked) {
        out.push_back(tracked.get());
    }
    return out;
}

void ActorRegistry_Clear() {
    sTracked.clear();
    sByKey.clear();
    sColliderOwner.clear();
    sSpawnCtx.clear();
    sInitializing.clear();
    sExpecting = false;
    sVersion++;
}

uint32_t ActorRegistry_Version() {
    return sVersion;
}

bool ActorRegistry_IsCinemaSpawn(uint32_t parentKey, const SpawnInfo& s) {
    TrackedActor* parent = parentKey != 0 ? ActorRegistry_Find(parentKey) : nullptr;
    return Rules_IsCutsceneActor((int16_t)s.actorId) || (parent != nullptr && parent->cinema);
}

void ActorRegistry_ExpectReplica(uint32_t key, uint32_t parentKey, uint32_t rootKey, int8_t room, const SpawnInfo& s,
                                 uint8_t owner) {
    sExpecting = true;
    sExpected = SpawnCtx{ true, key, parentKey, rootKey, room, true, s };
    sExpected.cinema = ActorRegistry_IsCinemaSpawn(parentKey, s);
    sExpected.owner = owner;
}

void ActorRegistry_EndExpect() {
    sExpecting = false;
}

Actor* ActorRegistry_SpawnUntracked(int16_t id, const Vec3f& pos, s16 params, s16 rotY) {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    sEchoing = true;
    Actor* a = Actor_Spawn(&gPlayState->actorCtx, gPlayState, id, pos.x, pos.y, pos.z, 0, rotY, 0, params);
    sEchoing = false;
    return a;
}

bool ActorRegistry_Adopt(Actor* actor, uint32_t key, uint8_t owner) {
    if (actor == nullptr || sTracked.count(actor) != 0 || sByKey.count(key) != 0 || InstanceSize(actor) == 0) {
        return false;
    }
    auto t = std::make_unique<TrackedActor>();
    t->actor = actor;
    t->key = key;
    t->rootKey = key;
    t->room = image_limits::kPlayerRoom;
    t->owner = owner;
    t->adopted = true;
    TrackedActor* raw = t.get();
    sByKey[key] = raw;
    sTracked[actor] = std::move(t);
    sVersion++;
    BuildRegions(*raw); // its instance (its skeletons and colliders were set up long ago: they stay its own)
    return true;
}

void ActorRegistry_ReleaseAdopted(Actor* actor) {
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t != nullptr && t->adopted) {
        Untrack(actor);
    }
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnActorSpawned(Actor* actor) {
    if (!WorldSession_InWorld() || gPlayState == nullptr || sEchoing || EndingMode_Active()) {
        return;
    }
    PropSync_OnSpawned(actor); // an item a pot drops is seen by everyone
    if (sExpecting) { // the copy of another game's runtime actor (ActorSync.cpp)
        sExpecting = false;
        sSpawnCtx[actor] = sExpected;
        return;
    }
    uint8_t category = actor->category;
    if (TrackedActor* parent = Current()) { // created in a replicated actor's Init: every game does the same
        uint32_t key = DerivedKey(parent->key, parent->initChildren++);
        Replication rule = Rules_RuntimeChild(actor->id, category);
        SpawnCtx c{ rule == Replication::Replicated || rule == Replication::Cinema, key, parent->key, parent->rootKey,
                    parent->room, false, {} };
        c.cinema = rule == Replication::Cinema || parent->cinema;
        c.owner = parent->owner;
        sSpawnCtx[actor] = c;
        return;
    }
    TrackedActor* updating = ActorSync_Updating();
    if (updating == nullptr) {
        // Our Link's arrows, bombs, hookshot... (Sync/PlayerObjects.cpp): the others show a copy, wherever they fly
        Player* link = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
        bool byLink = link != nullptr && (ActorSync_AnyUpdating() == &link->actor || actor->parent == &link->actor);
        if (byLink && !HostMode_Enabled() && PlayerObjects_Listed(actor->id) && Sync_On(SyncPart::PlayerObjects)) {
            SpawnInfo s;
            s.actorId = (uint16_t)actor->id;
            s.params = actor->params;
            s.pos[0] = actor->home.pos.x;
            s.pos[1] = actor->home.pos.y;
            s.pos[2] = actor->home.pos.z;
            s.rot[0] = actor->home.rot.x;
            s.rot[1] = actor->home.rot.y;
            s.rot[2] = actor->home.rot.z;
            uint32_t key = kRuntimeKeyBit | ((uint32_t)(Session_LocalId() & 0x3F) << 24) | (++sRuntimeSeq & 0xFFFFFF);
            SpawnCtx c{ true, key, 0, key, image_limits::kPlayerRoom, true, s };
            c.owner = Session_LocalId();
            sSpawnCtx[actor] = c;
        }
        return; // a list actor (decided in its Init) or never replicated (Epona: Ride.cpp tracks her on mount)
    }
    Replication rule = Rules_RuntimeChild(actor->id, category);
    if (rule == Replication::Echo && !updating->cinema) {
        SendEcho(actor, updating->room); // every other game creates its own; ours stays ours
    }
    if (rule != Replication::Replicated && rule != Replication::Cinema) {
        sSpawnCtx[actor] = SpawnCtx{};
        return;
    }
    SpawnInfo s;
    s.actorId = (uint16_t)actor->id;
    s.params = actor->params;
    s.pos[0] = actor->home.pos.x;
    s.pos[1] = actor->home.pos.y;
    s.pos[2] = actor->home.pos.z;
    s.rot[0] = actor->home.rot.x;
    s.rot[1] = actor->home.rot.y;
    s.rot[2] = actor->home.rot.z;
    s.parentKey = updating->key;
    uint32_t key = kRuntimeKeyBit | ((uint32_t)(Session_LocalId() & 0x3F) << 24) | (++sRuntimeSeq & 0xFFFFFF);
    SpawnCtx c{ true, key, updating->key, updating->rootKey, updating->room, true, s };
    c.cinema = rule == Replication::Cinema || updating->cinema;
    c.owner = updating->owner;
    sSpawnCtx[actor] = c;
}

static void InitBegin(Actor* actor) {
    sInitializing.push_back(nullptr);
    if (!WorldSession_InWorld() || EndingMode_Active() || actor->overlayEntry == nullptr ||
        actor->overlayEntry->profile == nullptr) {
        sSpawnCtx.erase(actor);
        return;
    }
    auto ctx = sSpawnCtx.find(actor);
    if (ctx != sSpawnCtx.end()) {
        SpawnCtx c = ctx->second;
        sSpawnCtx.erase(ctx);
        if (c.tracked) {
            Track(actor, c);
        }
        return;
    }
    int16_t index = GetActorListIndex(actor);
    Replication rule = Rules_ListActor(actor->id, actor->overlayEntry->profile->type);
    if (index < 0 || actor->room < 0 || actor->room > image_limits::kRoomMax ||
        (rule != Replication::Replicated && rule != Replication::Cinema)) {
        return;
    }
    uint32_t key = ListKey(actor->room, index);
    SpawnCtx c{ true, key, 0, key, actor->room, false, {} };
    c.cinema = rule == Replication::Cinema;
    Track(actor, c);
}

extern "C" void Coop_ActorInitBegin(Actor* actor) {
    InitBegin(actor);
    FlagReload_InitBegin(actor); // the flags it reads now are "read when created" (Sync/FlagReload.cpp)
    CopyCode_Begin(actor, true); // after it: a copy we create is tracked already (Sync/CopyCode.cpp)
}

static void InitEnd(Actor* actor);

extern "C" void Coop_ActorInitEnd(Actor* actor) {
    CopyCode_End();
    FlagReload_InitEnd();
    InitEnd(actor);
}

static void InitEnd(Actor* actor) {
    if (sInitializing.empty()) {
        return;
    }
    TrackedActor* t = sInitializing.back();
    sInitializing.pop_back();
    if (t == nullptr || t->actor != actor) {
        return;
    }
    if (actor->update == nullptr) {
        Untrack(actor); // killed itself in its Init (a flag says it is gone)
        return;
    }
    BuildRegions(*t);
    for (Collider* c : t->colliders) {
        sColliderOwner[c] = t;
    }
    SPDLOG_DEBUG("[Coop] Replicated actor {:#x} key {:#x}{} room {}: {} bytes, {} regions, {} colliders",
                 (uint16_t)actor->id, t->key, t->runtime ? " (runtime)" : "", (int)t->room, t->regions[0].size,
                 t->regions.size(), t->colliders.size());
}

extern "C" void Coop_OnSkelAnimeInit(SkelAnime* skelAnime) {
    TrackedActor* t = Current();
    if (t != nullptr && t->skels.size() < 4 && Inside(t->actor, skelAnime, sizeof(SkelAnime)) &&
        std::find(t->skels.begin(), t->skels.end(), skelAnime) == t->skels.end()) {
        t->skels.push_back(skelAnime); // its tables are read at the end of the Init (they are allocated after this)
    }
}

extern "C" void Coop_OnColliderSet(Collider* collider) {
    TrackedActor* t = Current();
    if (t == nullptr || t->colliders.size() >= (size_t)image_limits::kColliders ||
        !Inside(t->actor, collider, sizeof(Collider)) ||
        std::find(t->colliders.begin(), t->colliders.end(), collider) != t->colliders.end()) {
        return;
    }
    t->colliders.push_back(collider);
}

// Which colliders an actor put into this frame's collision pass: its copies register the same ones.
extern "C" void Coop_OnColliderRegistered(Collider* collider, s32 ac) {
    auto it = sColliderOwner.find(collider);
    if (it == sColliderOwner.end()) {
        return;
    }
    TrackedActor* t = it->second;
    for (size_t i = 0; i < t->colliders.size(); i++) {
        if (t->colliders[i] == collider) {
            (ac ? t->acMask : t->ocMask) |= (uint8_t)(1u << i);
            return;
        }
    }
}

// The Init of a replicated actor names a table of its own outside its instance (a boss's effects, the treasure
// maze): it travels as one more region.
extern "C" void Coop_AddActorRegion(Actor* actor, void* table, u32 bytes) {
    TrackedActor* t = Current();
    if (t != nullptr && t->actor == actor && table != nullptr && bytes > 0) {
        t->extraRegions.push_back({ (uint8_t*)table, (uint32_t)bytes });
    }
}

static void RegisterActorRegistry() {
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) {
        sSpawnCtx.erase(actor);
        if (TrackedActor* t = ActorRegistry_Get(actor)) {
            if (Leases_IsRemote(*t)) {
                CopyCode_NoteDyingCopy(actor, t->owner); // its Destroy runs after this hook (Actor_Delete)
            }
            ActorSync_OnDestroyed(*t);
        }
        ActorSync_ForgetPointersTo(actor);
        Untrack(actor);
    });
    COND_HOOK(OnPlayDestroy, true, ActorRegistry_Clear);
}

COOP_ON_EVENT(actorEcho, coop::ev::kEcho, OnEcho);
static RegisterShipInitFunc sActorRegistryInit(RegisterActorRegistry);

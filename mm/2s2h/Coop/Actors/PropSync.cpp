// [COOP] Shared props: what one player breaks, cuts or picks up (pots, grass, crates, barrels, rocks, rupees lying in
// the room) is gone for everyone in the scene. Every game keeps simulating its own props; only their disappearance
// travels ("prop" events). Keys: a prop of the room's list is (room << 16 | its index in the list); the grass of a
// group (Obj_Mure) is the group's key + the grass number; the field grass of Obj_Grass (no actors: elements of one
// manager) is a hash of its position. The server remembers what is gone while somebody stays in the scene and tells
// the ones who arrive ("props"); when the scene empties everything grows back, as after leaving it in the original.
// What a prop drops when it breaks is shown to every game ("item"); the first one to pick it up gets it.
// A prop is "broken" when it removes itself during its own update (a room unloading also removes actors: not shared).
// Grass (En_Kusa) is the exception: cut grass stays as a stub (some grows back), so its "cut" flag is watched instead,
// and the other games cut theirs the same way (a hit on its collider: stub, leaves and sound) without its drop.
// To share another prop: add its actor id and break sound to kSharedProps.
#include "PropSync.h"

#include "ActorSync.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <cmath>
#include <map>
#include <set>
#include <utility>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "overlays/actors/ovl_Obj_Grass/z_obj_grass.h"
#include "overlays/actors/ovl_Obj_Mure/z_obj_mure.h"
}

namespace coop::client {

namespace {

constexpr int kChildSingle = -1; // a prop of the room's list
constexpr int kChildGrass = -2;  // an element of the field grass (Obj_Grass)
constexpr int kChildItem = -3;   // an item a prop dropped
constexpr int kChildRecut = -4;  // grass that grows back was cut (never remembered: it grows back anyway)
constexpr uint8_t kMureChildDead = 1; // OBJMURE_CHILD_STATE_DEAD (private to z_obj_mure.c)

// The props that are shared: the actor and the sound its break makes in the games that only see it go (0: silent).
struct SharedProp {
    int16_t actorId;
    uint16_t sfx;
};
const SharedProp kSharedProps[] = {
    { ACTOR_OBJ_TSUBO, NA_SE_EV_POT_BROKEN },      { ACTOR_OBJ_FLOWERPOT, NA_SE_EV_POT_BROKEN },
    { ACTOR_EN_KUSA, NA_SE_EV_PLANT_BROKEN },
    { ACTOR_OBJ_KIBAKO, NA_SE_EV_WOODBOX_BREAK },  { ACTOR_OBJ_KIBAKO2, NA_SE_EV_WOODBOX_BREAK },
    { ACTOR_OBJ_TARU, NA_SE_EV_WOODBOX_BREAK },    { ACTOR_EN_ISHI, NA_SE_EV_ROCK_BROKEN },
    { ACTOR_OBJ_BOMBIWA, NA_SE_EV_WALL_BROKEN },   { ACTOR_OBJ_HAMISHI, NA_SE_EV_WALL_BROKEN },
    { ACTOR_OBJ_HUGEBOMBIWA, NA_SE_EV_WALL_BROKEN }, { ACTOR_OBJ_SNOWBALL, NA_SE_EV_WALL_BROKEN },
    { ACTOR_OBJ_SNOWBALL2, NA_SE_EV_WALL_BROKEN }, { ACTOR_EN_ITEM00, 0 },
};

const SharedProp* PropOf(int16_t id) {
    for (const SharedProp& p : kSharedProps) {
        if (p.actorId == id) {
            return &p;
        }
    }
    return nullptr;
}

uint32_t ListKey(int8_t room, int16_t index) {
    return ((uint32_t)(uint8_t)room << 16) | (uint16_t)index;
}

uint32_t GrassKey(const Vec3f& pos) {
    uint32_t h = 2166136261u;
    for (float v : { pos.x, pos.y, pos.z }) {
        uint32_t i = (uint32_t)(int32_t)std::lround(v);
        for (int b = 0; b < 4; b++) {
            h = (h ^ ((i >> (8 * b)) & 0xFF)) * 16777619u;
        }
    }
    return h & 0x7FFFFFFFu;
}

struct Mure {
    Actor* actor = nullptr;
    uint16_t known = 0; // grass we know is gone (received or sent): only new ones are sent
};

std::map<uint32_t, Actor*> sSingles;           // key -> prop of the room's list (loaded now)
std::map<const Actor*, uint32_t> sSingleKeys;  // actor -> key (a lifted pot leaves its room)
std::map<uint32_t, Mure> sMures;               // key -> grass group
std::map<uint32_t, Actor*> sItems;             // key -> shared item (ours or a copy)
std::map<const Actor*, uint32_t> sItemKeys;
std::set<std::pair<uint32_t, int>> sGone;      // (key, child) gone in this scene
std::set<uint32_t> sGrassKnown;                // field grass we know is gone
int16_t sAskedScene = -1;                      // the scene whose list we asked for
uint32_t sItemSeq = 0;
bool sApplying = false; // removing or creating what another game said: never sent back
std::set<const Actor*> sCutting;          // grass we cut because another game did: no drop, not sent back
std::map<const Actor*, bool> sKusaWasCut; // grass of the room's list: its cut flag last frame

bool Active() {
    return WorldSession_Active() && gPlayState != nullptr && !EndingMode_Active();
}

void Forget() {
    sSingles.clear();
    sSingleKeys.clear();
    sMures.clear();
    sItems.clear();
    sItemKeys.clear();
    sGone.clear();
    sGrassKnown.clear();
    sCutting.clear();
    sKusaWasCut.clear();
    sAskedScene = -1;
}

bool IsKusaGrass(const Actor* actor) {
    return actor->id == ACTOR_EN_KUSA && KUSA_GET_TYPE(actor) != ENKUSA_TYPE_BUSH; // a bush is removed when cut
}

// Cuts our copy of grass another player cut: a hit on its collider makes its own update cut it (stub, leaves, sound).
void Cut(Actor* actor) {
    EnKusa* kusa = (EnKusa*)actor;
    if (kusa->isCut || actor->update == nullptr) {
        return;
    }
    kusa->collider.base.acFlags |= AC_HIT;
    sCutting.insert(actor);
}

void SendGone(uint32_t key, int child) {
    SPDLOG_INFO("[Coop] Prop gone here: key {:#x} child {}", key, child);
    json ev = MakeEvent(ev::kProp);
    ev["scene"] = gPlayState->sceneId;
    ev["key"] = key;
    ev["child"] = child;
    NetClient::Get().SendEvent(ev);
}

void Remove(Actor* actor, bool withSound) {
    const SharedProp* prop = PropOf(actor->id);
    if (withSound && prop != nullptr && prop->sfx != 0) {
        SoundSource_PlaySfxAtFixedWorldPos(gPlayState, &actor->world.pos, 20, prop->sfx);
    }
    sApplying = true;
    Actor_Kill(actor);
    sApplying = false;
}

// Another game said this is gone: remove it from what is loaded now.
void Apply(uint32_t key, int child) {
    if (child == kChildSingle || child == kChildRecut) {
        auto it = sSingles.find(key);
        if (it != sSingles.end() && it->second->update != nullptr) {
            if (IsKusaGrass(it->second)) {
                Cut(it->second);
            } else {
                Remove(it->second, true);
            }
        }
    } else if (child == kChildItem) {
        auto it = sItems.find(key);
        if (it != sItems.end() && it->second->update != nullptr) {
            Remove(it->second, false);
        }
    } else if (child == kChildGrass) {
        sGrassKnown.insert(key); // FrameEnd removes it from the manager
    } else {
        auto it = sMures.find(key);
        if (it == sMures.end() || child < 0 || child >= OBJMURE_MAX_SPAWNS) {
            return;
        }
        ObjMure* mure = (ObjMure*)it->second.actor;
        it->second.known |= (uint16_t)(1u << child);
        mure->childrenStates[child] = kMureChildDead;
        if (mure->children[child] != nullptr) {
            if (mure->children[child]->update != nullptr) {
                Remove(mure->children[child], true);
            }
            mure->children[child] = nullptr;
        }
    }
}

void Note(uint32_t key, int child) {
    if (sGone.insert({ key, child }).second) {
        Apply(key, child);
    }
}

void OnActorInit(Actor* actor) {
    if (!Active() || actor->update == nullptr) {
        return;
    }
    int16_t index = GetActorListIndex(actor);
    if (index < 0 || actor->room < 0) {
        return;
    }
    uint32_t key = ListKey(actor->room, index);
    if (actor->id == ACTOR_OBJ_MURE) {
        if (OBJ_MURE_GET_TYPE(actor) == OBJMURE_TYPE_GRASS) {
            sMures[key].actor = actor;
        }
        return;
    }
    if (PropOf(actor->id) == nullptr) {
        return;
    }
    sSingles[key] = actor;
    sSingleKeys[actor] = key;
    if (IsKusaGrass(actor)) {
        sKusaWasCut[actor] = false;
    }
    if (sGone.count({ key, kChildSingle }) != 0) {
        if (IsKusaGrass(actor)) {
            Cut(actor); // cut before it loaded here
        } else {
            Remove(actor, false); // gone before it loaded here
        }
    }
}

// Removed during its own update (broken, cut, picked up): gone for everyone.
void OnActorKill(Actor* actor) {
    if (sApplying || !Active() || actor != ActorSync_AnyUpdating()) {
        return;
    }
    if (auto it = sSingleKeys.find(actor); it != sSingleKeys.end()) {
        if (sGone.insert({ it->second, kChildSingle }).second) {
            SendGone(it->second, kChildSingle);
        }
    } else if (auto item = sItemKeys.find(actor); item != sItemKeys.end()) {
        SendGone(item->second, kChildItem);
    }
}

void OnActorDestroy(Actor* actor) {
    if (auto it = sSingleKeys.find(actor); it != sSingleKeys.end()) {
        auto single = sSingles.find(it->second);
        if (single != sSingles.end() && single->second == actor) {
            sSingles.erase(single);
        }
        sSingleKeys.erase(it);
    }
    if (auto it = sItemKeys.find(actor); it != sItemKeys.end()) {
        sItems.erase(it->second);
        sItemKeys.erase(it);
    }
    sCutting.erase(actor);
    sKusaWasCut.erase(actor);
    for (auto m = sMures.begin(); m != sMures.end(); ++m) {
        if (m->second.actor == actor) {
            sMures.erase(m);
            break;
        }
    }
}

void PollMures() {
    for (auto& [key, m] : sMures) {
        ObjMure* mure = (ObjMure*)m.actor;
        for (int i = 0; i < OBJMURE_MAX_SPAWNS; i++) {
            uint16_t bit = (uint16_t)(1u << i);
            bool dead = mure->childrenStates[i] == kMureChildDead;
            if (dead && !(m.known & bit)) {
                m.known |= bit;
                if (sGone.insert({ key, i }).second) {
                    SendGone(key, i);
                }
            } else if (!dead && sGone.count({ key, i }) != 0) {
                Apply(key, i); // cut by someone else before this group was loaded
            }
        }
    }
}

// The field grass: elements of one manager (per scene), grouped by the rooms that loaded them.
void PollGrass() {
    for (Actor* a = gPlayState->actorCtx.actorLists[ACTORCAT_PROP].first; a != nullptr; a = a->next) {
        if (a->id != ACTOR_OBJ_GRASS || a->update == nullptr) {
            continue;
        }
        ObjGrass* grass = (ObjGrass*)a;
        for (s32 g = 0; g < grass->activeGrassGroups && g < ARRAY_COUNT(grass->grassGroups); g++) {
            ObjGrassGroup* group = &grass->grassGroups[g];
            for (s32 e = 0; e < group->count && e < OBJ_GRASS_GROUP_ELEM_COUNT_MAX; e++) {
                ObjGrassElement* elem = &group->elements[e];
                uint32_t key = GrassKey(elem->pos);
                bool removed = (elem->flags & OBJ_GRASS_ELEM_REMOVED) != 0;
                if (removed && sGrassKnown.insert(key).second) {
                    if (sGone.insert({ key, kChildGrass }).second) {
                        SendGone(key, kChildGrass);
                    }
                } else if (!removed && sGrassKnown.count(key) != 0) {
                    elem->flags |= OBJ_GRASS_ELEM_REMOVED; // cut by someone else
                }
            }
        }
    }
}

// Grass of the room's list that was just cut (by us: sent; by another game's order: done).
void PollKusa() {
    for (auto& [actor, wasCut] : sKusaWasCut) {
        bool cut = ((const EnKusa*)actor)->isCut != 0;
        if (cut && !wasCut && sCutting.erase(actor) == 0) {
            auto key = sSingleKeys.find(actor);
            if (key != sSingleKeys.end()) {
                if (KUSA_GET_TYPE(actor) == ENKUSA_TYPE_REGROWING_GRASS) {
                    SendGone(key->second, kChildRecut); // it grows back: the others cut theirs, nobody remembers
                } else if (sGone.insert({ key->second, kChildSingle }).second) {
                    SendGone(key->second, kChildSingle);
                }
            }
        }
        wasCut = cut;
    }
}

void FrameEnd() {
    if (!Active() || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    if (sAskedScene != gPlayState->sceneId) {
        sAskedScene = gPlayState->sceneId; // just arrived: what did the others break here?
        json ev = MakeEvent(ev::kProps);
        ev["scene"] = gPlayState->sceneId;
        NetClient::Get().SendEvent(ev);
    }
    PollMures();
    PollGrass();
    PollKusa();
}

bool ReadEntry(int64_t k, int64_t c, uint32_t& key, int& child) {
    if (k < 0 || k > 0xFFFFFFFFll || c < kChildRecut || c >= OBJMURE_MAX_SPAWNS) {
        return false;
    }
    key = (uint32_t)k;
    child = (int)c;
    return true;
}

void OnProp(const json& ev) {
    uint32_t key;
    int child;
    if (!Active() || GetInt(ev, "scene", -1) != gPlayState->sceneId ||
        !ReadEntry(GetInt(ev, "key", -1), GetInt(ev, "child", -9), key, child)) {
        return;
    }
    SPDLOG_INFO("[Coop] Prop gone in player {}'s game: key {:#x} child {}", GetInt(ev, "from"), key, child);
    if (child == kChildItem || child == kChildRecut) {
        Apply(key, child); // never remembered
    } else {
        Note(key, child);
    }
}

// What is gone in the scene we just entered: [[key, child], ...].
void OnProps(const json& ev) {
    auto list = ev.find("list");
    if (!Active() || GetInt(ev, "scene", -1) != gPlayState->sceneId || list == ev.end() || !list->is_array()) {
        return;
    }
    for (const json& e : *list) {
        uint32_t key;
        int child;
        if (e.is_array() && e.size() == 2 && e[0].is_number_integer() && e[1].is_number_integer() &&
            ReadEntry(e[0].get<int64_t>(), e[1].get<int64_t>(), key, child) && child != kChildItem &&
            child != kChildRecut) {
            Note(key, child);
        }
    }
}

// An item another game's prop dropped: ours to see and to pick up.
void OnItem(const json& ev) {
    auto pos = ev.find("pos");
    int64_t key = GetInt(ev, "key", -1);
    if (!Active() || GetInt(ev, "scene", -1) != gPlayState->sceneId || GetInt(ev, "id", -1) != ACTOR_EN_ITEM00 ||
        key < 0 || key > 0xFFFFFFFFll || pos == ev.end() || !pos->is_array() || pos->size() != 3) {
        return;
    }
    f32 p[3];
    for (int i = 0; i < 3; i++) {
        if (!(*pos)[i].is_number() || !std::isfinite((*pos)[i].get<double>())) {
            return;
        }
        p[i] = (f32)(*pos)[i].get<double>();
    }
    // Fixed items never carry a collectible flag across games (0x7F00: the flag of a unique pickup; LiveFlags.cpp).
    s32 params = (s32)(GetInt(ev, "params") & ~0x7F00);
    sApplying = true;
    Actor* item = Actor_Spawn(&gPlayState->actorCtx, gPlayState, ACTOR_EN_ITEM00, p[0], p[1], p[2], 0, 0, 0, params);
    sApplying = false;
    SPDLOG_INFO("[Coop] Item dropped in player {}'s game: key {:#x} params {:#x} ({})", GetInt(ev, "from"),
                (uint32_t)key, params, item != nullptr ? "shown" : "not created");
    if (item != nullptr) {
        sItems[(uint32_t)key] = item;
        sItemKeys[item] = (uint32_t)key;
    }
}

void RegisterPropSync() {
    COND_HOOK(OnActorInit, true, OnActorInit);
    COND_HOOK(OnActorKill, true, OnActorKill);
    COND_HOOK(OnActorDestroy, true, OnActorDestroy);
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

void PropSync_OnSpawned(Actor* actor) {
    if (sApplying || !Active() || actor->id != ACTOR_EN_ITEM00 || gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    Actor* from = ActorSync_AnyUpdating();
    if (from == nullptr || from == actor || (from->id != ACTOR_OBJ_GRASS && PropOf(from->id) == nullptr) ||
        from->id == ACTOR_EN_ITEM00) {
        return; // only what a prop drops (enemies: DropSync.cpp gives every player their own)
    }
    uint32_t key = 0x80000000u | ((uint32_t)(Session_LocalId() & 0x3F) << 24) | (++sItemSeq & 0xFFFFFF);
    sItems[key] = actor;
    sItemKeys[actor] = key;
    json ev = MakeEvent(ev::kItem);
    ev["scene"] = gPlayState->sceneId;
    ev["key"] = key;
    ev["id"] = actor->id;
    ev["params"] = actor->params;
    ev["pos"] = { actor->world.pos.x, actor->world.pos.y, actor->world.pos.z };
    NetClient::Get().SendEvent(ev);
    SPDLOG_INFO("[Coop] Item dropped here: key {:#x} params {:#x}", key, (uint16_t)actor->params);
}

COOP_ON_EVENT(propEvent, coop::ev::kProp, OnProp);
COOP_ON_EVENT(propsEvent, coop::ev::kProps, OnProps);
COOP_ON_EVENT(itemEvent, coop::ev::kItem, OnItem);
COOP_ON_LOST(propLost, [](const std::string&) { Forget(); });
static RegisterShipInitFunc sPropSyncInit(RegisterPropSync);

} // namespace coop::client

// z_en_item00.c: nothing drops from grass we cut because another player did (they got its drop).
extern "C" s32 Coop_DropSuppressed(void) {
    using namespace coop::client;
    Actor* updating = ActorSync_AnyUpdating();
    return (updating != nullptr && sCutting.count(updating) != 0) ? 1 : 0;
}

// [COOP] Drops compartidos (spec docs/superpowers/specs/2026-10-07-coop-drops-compartidos-design.md): every item that
// falls while playing in the server's world (rupees, hearts, magic, ammo, keys, fairies, stray fairies) is one item for
// the whole stage. The game where it falls announces it at the end of that frame, with how it moves ("item"); the
// others create the same item, which flies and lands the same way ("item_rest" corrects where it lies). Whoever touches
// one first gets it: the server says who ("item_take"), the others remove theirs ("item_gone"). The server keeps what
// lies in each stage for whoever arrives later ("items"). The items of a room's list are the same in every game: only
// who takes them travels. DropRules.cpp says which actors are shared items and which spawners drop twins.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/ItemState.h"
#include "common/PlayerState.h"
#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ObjectExtension/ActorListIndex.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <set>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kTakeTimeoutMs = 3000; // no answer from the server: the item is ours
constexpr float kRestSnap = 1.0f;        // a copy lying further than this from where its dropper's lies moves there
constexpr uint8_t kStillFrames = 2;      // frames lying still before we say where it rests

enum class Claim : uint8_t { None, Asked, Granted };

struct Item {
    Actor* actor = nullptr;
    bool mine = false;   // we announced it: we say where it comes to lie
    bool restSent = false;
    bool hasRest = false; // its dropper said where it lies: our copy goes there once it lies too
    Vec3f rest = {};
    Claim claim = Claim::None;
    int64_t askedMs = 0;
    ActorFunc draw = nullptr; // its draw while hidden (asked)
    uint8_t still = 0;
};

// Created this frame: announced at its end, when whoever dropped it has set how it moves.
struct Fresh {
    Actor* actor = nullptr;
    s32 params = 0;           // its params when created (its Init may change them)
    Actor* spawner = nullptr; // whose update created it (nullptr once it is gone)
    int16_t spawnerId = -1;
    uint32_t spawnerSeq = 0; // the how-manyth shared item that spawner made in this scene
    bool list = false;       // an item of the room's list: every game has it, nothing to announce
    uint32_t listKey = 0;
};

std::unordered_map<uint32_t, Item> sItems;           // key -> our item for it
std::unordered_map<const Actor*, uint32_t> sKeys;    // our item -> key
std::vector<Fresh> sFresh;
std::set<uint32_t> sTaken;                           // keys somebody took in this scene
std::unordered_map<const Actor*, uint32_t> sSpawned; // spawner -> shared items it made in this scene
uint32_t sSeq = std::random_device{}() & 0xFFFFFF;   // our unique keys: a random start, so a player who comes back
                                                     // with the same id never reuses keys the server remembers
bool sCreating = false;                              // creating a copy: not a new item
int16_t sAskedScene = -1;                            // the scene whose "items" we asked for

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool On() {
    return Sync_On(SyncPart::Drops) && gPlayState != nullptr && !EndingMode_Active();
}

Player* LocalLink() {
    return (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool Alive(const Actor* a) {
    return a != nullptr && a->update != nullptr;
}

void Forget() {
    sItems.clear();
    sKeys.clear();
    sFresh.clear();
    sTaken.clear();
    sSpawned.clear();
    sAskedScene = -1;
}

json Event(const char* type) {
    json ev = MakeEvent(type);
    ev["scene"] = gPlayState->sceneId;
    return ev;
}

bool HereNow(const json& ev) {
    return On() && GetInt(ev, "scene", -1) == gPlayState->sceneId;
}

bool ReadKey(const json& ev, uint32_t& key) {
    int64_t k = GetInt(ev, "key", -1);
    if (k < 0 || k > 0xFFFFFFFFll) {
        return false;
    }
    key = (uint32_t)k;
    return true;
}

void Bind(uint32_t key, Actor* actor, bool mine) {
    Item it;
    it.actor = actor;
    it.mine = mine;
    sItems[key] = it;
    sKeys[actor] = key;
}

void Unbind(const Actor* actor) {
    auto k = sKeys.find(actor);
    if (k == sKeys.end()) {
        return;
    }
    auto it = sItems.find(k->second);
    if (it != sItems.end() && it->second.actor == actor) {
        sItems.erase(it);
    }
    sKeys.erase(k);
}

// Our item of a key somebody else has (they took it, or a twin of ours stays): it just vanishes.
void Remove(Actor* actor) {
    Unbind(actor);
    if (Alive(actor)) {
        Actor_Kill(actor);
    }
}

bool Lying(Actor* a) {
    return a->id == ACTOR_EN_ITEM00 && EnItem00_CoopGetAction((EnItem00*)a) == 0 &&
           (a->bgCheckFlags & BGCHECKFLAG_GROUND) && std::fabs(a->speed) < 0.1f;
}

float Clamp(float v, float limit) {
    return std::isfinite(v) ? std::clamp(v, -limit, limit) : 0.f;
}

// What travels of an item that fell here: what it is and how it moves now.
ItemState Capture(const Fresh& f, uint32_t key) {
    ItemState s;
    Actor* a = f.actor;
    s.key = key;
    s.id = a->id;
    s.params = f.params & 0xFFFF;
    if (s.id == ACTOR_EN_ITEM00) {
        EnItem00* item = (EnItem00*)a;
        s.pos[0] = a->world.pos.x;
        s.pos[1] = a->world.pos.y;
        s.pos[2] = a->world.pos.z;
        s.vy = Clamp(a->velocity.y, item_limits::kMotion);
        s.speed = Clamp(a->speed, item_limits::kMotion);
        s.grav = Clamp(a->gravity, item_limits::kMotion);
        s.scale = std::clamp(Clamp(a->scale.x, item_limits::kScale), 0.f, item_limits::kScale);
        s.yaw = a->world.rot.y;
        s.phase = a->home.rot.z;
        s.timer = item->unk152;
        s32 act = EnItem00_CoopGetAction(item);
        s.act = (uint8_t)(act < 0 ? 0 : act);
    } else {
        s.pos[0] = a->home.pos.x; // a fairy flies around where it was made
        s.pos[1] = a->home.pos.y;
        s.pos[2] = a->home.pos.z;
    }
    for (float& p : s.pos) {
        p = Clamp(p, pose_limits::kWorldLimit);
    }
    return s;
}

uint32_t KeyFor(const Fresh& f) {
    if (f.list) {
        return f.listKey;
    }
    if (f.spawner != nullptr && DropRules_IsTwinSpawner(f.spawnerId)) {
        uint32_t twin = DropRules_TwinKey(f.actor, f.spawner, f.spawnerSeq);
        if (twin != 0) {
            return twin;
        }
    }
    return MakeUniqueItemKey(Session_LocalId(), ++sSeq);
}

// Gives an item that appeared here its key: another game's twin may be here already, or the key was taken already.
void Settle(const Fresh& f) {
    uint32_t key = KeyFor(f);
    if (sTaken.count(key) != 0) {
        Actor_Kill(f.actor); // taken before it appeared here (an item of the list, a twin)
        return;
    }
    auto it = sItems.find(key);
    if (it != sItems.end() && Alive(it->second.actor) && it->second.actor != f.actor) {
        // Twins: one item per key. Ours stays (its spawner keeps an eye on it) unless that copy is being taken.
        if (it->second.claim != Claim::None) {
            Actor_Kill(f.actor);
            return;
        }
        Remove(it->second.actor);
    }
    Bind(key, f.actor, !f.list);
    if (!f.list) {
        json ev = Event(ev::kItem);
        WriteItemState(Capture(f, key), ev);
        NetClient::Get().SendEvent(ev);
        SPDLOG_INFO("[Coop] Item {:#x} dropped here (actor {:#x}, params {:#x})", key, (uint16_t)f.actor->id,
                    (uint16_t)f.params);
    }
}

void SettleFresh() {
    std::vector<Fresh> fresh;
    fresh.swap(sFresh);
    for (const Fresh& f : fresh) {
        if (Alive(f.actor) && sKeys.count(f.actor) == 0) {
            Settle(f);
        }
    }
}

// The key of a shared item, settling it first if it was created this frame (touched as soon as it appeared).
bool KeyOfNow(Actor* actor, uint32_t& key) {
    auto k = sKeys.find(actor);
    if (k == sKeys.end()) {
        auto f = std::find_if(sFresh.begin(), sFresh.end(), [&](const Fresh& x) { return x.actor == actor; });
        if (f == sFresh.end()) {
            return false;
        }
        Fresh fresh = *f;
        sFresh.erase(f);
        Settle(fresh);
        k = sKeys.find(actor);
        if (k == sKeys.end()) {
            return false;
        }
    }
    key = k->second;
    return true;
}

// The same item another game dropped: it moves as theirs did (lying: where it lies now, for whoever arrives late).
Actor* CreateCopy(const ItemState& s, bool lying, int16_t timer) {
    Vec3f pos = { s.pos[0], s.pos[1], s.pos[2] };
    bool collectible = s.id == ACTOR_EN_ITEM00;
    // Without its collectible flag: whether it still lies there is the server's to say (the rupees of a formation all
    // share one flag); it gets the flag back below, and sets it when taken as always.
    s32 params = collectible ? (s.params & ~0x7F00) : s.params;
    sCreating = true;
    Actor* a = ActorRegistry_SpawnUntracked(s.id, pos, (s16)params, collectible ? s.yaw : 0);
    sCreating = false;
    if (!Alive(a)) {
        return nullptr; // its Init removed it (a fairy whose flag says it was taken)
    }
    if (collectible) {
        EnItem00* item = (EnItem00*)a;
        item->collectibleFlag = (s16)((s.params & 0x7F00) >> 8);
        a->room = -1; // it goes along when we change rooms, like what falls
        a->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        a->world.rot.y = s.yaw;
        a->home.rot.z = s.phase;
        a->gravity = s.grav;
        if (EnItem00_CoopGetAction(item) == 0) { // not one waiting for its object (a heart container)
            EnItem00_CoopSetAction(item, lying ? 0 : s.act);
        }
        if (!lying) {
            a->velocity.y = s.vy;
            a->speed = s.speed;
            Actor_SetScale(a, s.scale);
        }
        item->unk152 = timer;
        a->prevPos = a->world.pos;
    }
    return a;
}

void Grant(Item& it) {
    it.claim = Claim::Granted;
    if (Alive(it.actor) && it.draw != nullptr) {
        it.actor->draw = it.draw;
    }
}

// ---- Events ----

void OnItem(const json& ev) {
    ItemState s;
    if (!HereNow(ev) || HostMode_Enabled() || !ReadItemState(ev, s) || sTaken.count(s.key) != 0) {
        return;
    }
    auto it = sItems.find(s.key);
    if (it != sItems.end() && Alive(it->second.actor)) {
        return; // a twin of ours is here already
    }
    Actor* a = CreateCopy(s, false, s.timer);
    SPDLOG_INFO("[Coop] Item {:#x} dropped in player {}'s game ({})", s.key, GetInt(ev, "from"),
                a != nullptr ? "shown" : "not created");
    if (a != nullptr) {
        Bind(s.key, a, false);
    }
}

// What lies in the scene we just entered, and the keys taken there.
void OnItems(const json& ev) {
    if (!HereNow(ev)) {
        return;
    }
    auto taken = ev.find("taken");
    if (taken != ev.end() && taken->is_array()) {
        for (const json& k : *taken) {
            if (!k.is_number_integer() || k.get<int64_t>() < 0 || k.get<int64_t>() > 0xFFFFFFFFll) {
                continue;
            }
            uint32_t key = (uint32_t)k.get<int64_t>();
            sTaken.insert(key);
            auto it = sItems.find(key);
            if (it != sItems.end()) {
                Remove(it->second.actor);
            }
        }
    }
    auto list = ev.find("list");
    if (list == ev.end() || !list->is_array()) {
        return;
    }
    int shown = 0;
    for (const json& e : *list) {
        ItemState s;
        if (!e.is_object() || !ReadItemState(e, s) || sTaken.count(s.key) != 0) {
            continue;
        }
        auto it = sItems.find(s.key);
        if (it != sItems.end() && Alive(it->second.actor)) {
            continue;
        }
        int16_t timer = s.timer;
        if (s.id == ACTOR_EN_ITEM00 && s.timer > 0) {
            int64_t left = (int64_t)s.timer - GetInt(e, "age") / 50;
            if (left <= 0) {
                continue; // it vanished already
            }
            timer = (int16_t)left;
        }
        if (Actor* a = CreateCopy(s, true, timer)) {
            Bind(s.key, a, false);
            shown++;
        }
    }
    SPDLOG_INFO("[Coop] Items lying here when we arrived: {} shown", shown);
}

void OnTakeAnswer(const json& ev) {
    uint32_t key;
    if (!HereNow(ev) || !ReadKey(ev, key)) {
        return;
    }
    auto it = sItems.find(key);
    if (it == sItems.end() || it->second.claim != Claim::Asked) {
        return;
    }
    if (GetBool(ev, "ok")) {
        SPDLOG_INFO("[Coop] Item {:#x} is ours", key);
        Grant(it->second);
    } else {
        SPDLOG_INFO("[Coop] Item {:#x}: another player took it first", key);
        sTaken.insert(key);
        Remove(it->second.actor);
    }
}

void OnGone(const json& ev) {
    uint32_t key;
    if (!HereNow(ev) || !ReadKey(ev, key)) {
        return;
    }
    sTaken.insert(key);
    auto it = sItems.find(key);
    if (it != sItems.end()) {
        SPDLOG_INFO("[Coop] Item {:#x} taken by player {}", key, GetInt(ev, "from"));
        Remove(it->second.actor);
    }
}

void OnRest(const json& ev) {
    uint32_t key;
    auto pos = ev.find("pos");
    if (!HereNow(ev) || !ReadKey(ev, key) || pos == ev.end() || !pos->is_array() || pos->size() != 3) {
        return;
    }
    float p[3];
    for (int i = 0; i < 3; i++) {
        if (!(*pos)[i].is_number() || !std::isfinite((*pos)[i].get<double>())) {
            return;
        }
        p[i] = (float)(*pos)[i].get<double>();
    }
    auto it = sItems.find(key);
    if (it == sItems.end() || it->second.mine) {
        return;
    }
    it->second.hasRest = true;
    it->second.rest = { p[0], p[1], p[2] };
}

// ---- Every frame ----

void AskOnArrival() {
    if (sAskedScene == gPlayState->sceneId || HostMode_Enabled()) {
        return;
    }
    sAskedScene = gPlayState->sceneId;
    json ev = Event(ev::kItems);
    ev["layer"] = (int)std::clamp<int>(gSaveContext.sceneLayer, 0, pose_limits::kMaxLayer); // server/Stage.h
    NetClient::Get().SendEvent(ev);
}

// Ours say where they came to lie; copies go where their dropper's lies.
void Watch() {
    for (auto& [key, it] : sItems) {
        if (!Alive(it.actor) || it.actor->id != ACTOR_EN_ITEM00) {
            continue;
        }
        if (it.mine && !it.restSent) {
            it.still = Lying(it.actor) ? (uint8_t)(it.still + 1) : 0;
            if (it.still >= kStillFrames) {
                it.restSent = true;
                json ev = Event(ev::kItemRest);
                ev["key"] = key;
                ev["pos"] = { it.actor->world.pos.x, it.actor->world.pos.y, it.actor->world.pos.z };
                NetClient::Get().SendEvent(ev);
            }
        } else if (it.hasRest && Lying(it.actor)) {
            it.hasRest = false;
            if (Math_Vec3f_DistXYZ(&it.actor->world.pos, &it.rest) > kRestSnap) {
                it.actor->world.pos = it.rest;
                it.actor->prevPos = it.rest;
            }
        }
    }
}

void FrameEnd() {
    if (!On()) {
        sFresh.clear();
        return;
    }
    if (gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    AskOnArrival();
    SettleFresh();
    Watch();
}

void OnActorDestroyed(Actor* actor) {
    Unbind(actor);
    sSpawned.erase(actor);
    sFresh.erase(std::remove_if(sFresh.begin(), sFresh.end(), [&](const Fresh& f) { return f.actor == actor; }),
                 sFresh.end());
    for (Fresh& f : sFresh) {
        if (f.spawner == actor) {
            f.spawner = nullptr;
        }
    }
}

void RegisterSharedDrops() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnActorDestroy, true, OnActorDestroyed);
    COND_HOOK(OnPlayDestroy, true, Forget);
}

} // namespace

bool SharedDrops_OnSpawned(Actor* actor) {
    if (sCreating || !On() || CopyCode_Running()) {
        return false;
    }
    Actor* spawner = ActorSync_AnyUpdating();
    DropKind kind = DropRules_Classify(actor, spawner);
    if (kind == DropKind::None) {
        return false;
    }
    Fresh f;
    f.actor = actor;
    f.params = actor->params;
    f.list = kind == DropKind::ListItem;
    if (f.list) {
        f.listKey = MakeListItemKey(actor->room, GetActorListIndex(actor));
    } else if (spawner != nullptr) {
        f.spawner = spawner;
        f.spawnerId = spawner->id;
        f.spawnerSeq = ++sSpawned[spawner];
    }
    sFresh.push_back(f);
    return true;
}

bool SharedDrops_Tracks(const Actor* actor) {
    return On() && sKeys.count(actor) != 0;
}

std::vector<SharedDropRow> SharedDrops_Nearby(float maxDist) {
    std::vector<SharedDropRow> rows;
    if (gPlayState == nullptr || LocalLink() == nullptr) {
        return rows;
    }
    Player* link = LocalLink();
    for (const auto& [key, it] : sItems) {
        if (!Alive(it.actor)) {
            continue;
        }
        float d = Actor_WorldDistXYZToActor(&link->actor, it.actor);
        if (d <= maxDist) {
            rows.push_back({ key, it.actor->id, it.mine, (uint8_t)it.claim, d });
        }
    }
    std::sort(rows.begin(), rows.end(), [](const SharedDropRow& a, const SharedDropRow& b) { return a.dist < b.dist; });
    return rows;
}

COOP_ON_EVENT(itemEvent, coop::ev::kItem, OnItem);
COOP_ON_EVENT(itemTakeEvent, coop::ev::kItemTake, OnTakeAnswer);
COOP_ON_EVENT(itemGoneEvent, coop::ev::kItemGone, OnGone);
COOP_ON_EVENT(itemRestEvent, coop::ev::kItemRest, OnRest);
COOP_ON_EVENT(itemsEvent, coop::ev::kItems, OnItems);
COOP_ON_LOST(itemsLost, [](const std::string&) { Forget(); });
static RegisterShipInitFunc sSharedDropsInit(RegisterSharedDrops);

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_ItemPrepare(Actor* actor) {
    auto k = sKeys.find(actor);
    if (k == sKeys.end() || gPlayState == nullptr) {
        return;
    }
    auto it = sItems.find(k->second);
    Player* link = LocalLink();
    if (it == sItems.end() || it->second.claim != Claim::Granted || link == nullptr) {
        return;
    }
    actor->world.pos = link->actor.world.pos; // ours: wherever Link went while the server answered
    actor->xzDistToPlayer = 0.0f;
    actor->playerHeightRel = 0.0f;
}

extern "C" s32 Coop_ItemTake(Actor* actor) {
    if (HostMode_Enabled() && WorldSession_InWorld()) {
        return 1; // the server's own game never takes anything
    }
    uint32_t key;
    if (!On() || !KeyOfNow(actor, key)) {
        return Alive(actor) ? 0 : 1; // not shared (or it was just removed: a twin of it is here, it was taken)
    }
    Item& it = sItems[key];
    if (it.claim == Claim::Granted) {
        return 0;
    }
    if (it.claim == Claim::Asked) {
        if (NowMs() - it.askedMs < kTakeTimeoutMs) {
            return 1;
        }
        SPDLOG_WARN("[Coop] Item {:#x}: no answer from the server, taken anyway", key);
        Grant(it);
        return 0;
    }
    it.claim = Claim::Asked;
    it.askedMs = NowMs();
    it.draw = actor->draw;
    actor->draw = nullptr;           // hidden while the server decides
    ((EnItem00*)actor)->unk152 = -1; // and it does not vanish meanwhile
    json ev = Event(ev::kItemTake);
    ev["key"] = key;
    NetClient::Get().SendEvent(ev);
    return 1;
}

extern "C" void Coop_FairyTaken(Actor* actor) {
    uint32_t key;
    if (!On() || HostMode_Enabled() || !KeyOfNow(actor, key)) {
        return;
    }
    Item& it = sItems[key];
    if (it.claim != Claim::None) {
        return;
    }
    it.claim = Claim::Granted; // used already: the server only tells the others
    json ev = Event(ev::kItemTake);
    ev["key"] = key;
    ev["after"] = true;
    NetClient::Get().SendEvent(ev);
}

extern "C" s16 Coop_ItemSwayYaw(Actor* actor) {
    return sKeys.count(actor) != 0 ? actor->world.rot.y : actor->yawTowardsPlayer;
}

extern "C" s32 Coop_KeepHeartDrops(void) {
    if (!On()) {
        return 0;
    }
    for (uint8_t id = 1; id <= kMaxPlayers; id++) {
        if (id != Session_LocalId() && PuppetManager_Actor(id) != nullptr) {
            return 1;
        }
    }
    return 0;
}

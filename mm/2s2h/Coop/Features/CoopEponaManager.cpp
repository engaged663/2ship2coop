#include "CoopEponaManager.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/EponaState.h"
#include "common/Protocol.h"
#include "common/StreamIds.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <map>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64horse.h"
#include "overlays/actors/ovl_En_Horse/z_en_horse.h"
#include "objects/object_horse_link_child/object_horse_link_child.h"
}

namespace coop::client {

namespace {

std::map<EponaKey, HorseProxyState> sHorses;
std::map<Actor*, EponaKey> sActorKeys;
std::map<EponaKey, uint8_t> sPassengers;
uint32_t sNextCallSequence = 1;
uint16_t sPacketSequence = 0;
int16_t sScene = -1;

// Scene changes on horseback (they survive the scene change, unlike the maps above):
// - the driver keeps the key of the horse it rode, so the horse the game gives it in the new scene is the same one;
// - a passenger arrives on foot (the game would give it its own horse) and gets back on the driver's horse there.
uint32_t sOwnRiddenSequence = 0; // our horse we are riding (0: none)
struct Rejoin {
    EponaKey key{ 0, 0 };
    int64_t untilMs = 0; // 0: nothing to rejoin
};
Rejoin sRejoin;
constexpr int64_t kRejoinWaitMs = 15000;

// Getting off another player's horse must not make it "our Epona" (the game saves the horse you get off as yours:
// after a restart every passenger would bring a copy of it). Saved when we get on, put back when we get off.
// Two players' Eponas always coexist, however close they get (each has its own owner): nothing ever merges them.
HorseData sHorseDataBeforeRide;
bool sHorseDataSaved = false;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

Player* LocalLink() {
    return gPlayState == nullptr ? nullptr : GET_PLAYER(gPlayState);
}

bool InGameplay(PlayState* play) {
    return play != nullptr && WorldSession_Active() && !EndingMode_Active() && gSaveContext.gameMode == GAMEMODE_NORMAL &&
           play->transitionTrigger == TRANS_TRIGGER_OFF;
}

bool IsHorseActor(const Actor* actor) {
    return actor != nullptr && actor->id == ACTOR_EN_HORSE;
}

bool IsMounted(const EnHorse* horse) {
    Player* player = LocalLink();
    return player != nullptr && player->rideActor == &horse->actor && (player->stateFlags1 & PLAYER_STATE1_800000);
}

EnHorse* FindAvailableOwnerHorse() {
    if (gPlayState == nullptr) {
        return nullptr;
    }
    for (Actor* actor = gPlayState->actorCtx.actorLists[ACTORCAT_BG].first; actor != nullptr; actor = actor->next) {
        if (!IsHorseActor(actor) || actor->update == nullptr || CoopEpona_IsProxy(actor)) {
            continue;
        }
        EnHorse* horse = (EnHorse*)actor;
        if (horse->type == HORSE_TYPE_2 && !sActorKeys.contains(actor) &&
            (horse->actor.params == ENHORSE_1 || horse->actor.params == ENHORSE_2)) {
            return horse;
        }
    }
    return nullptr;
}

void RegisterOwnerHorse(EnHorse* horse, uint32_t sequence) {
    if (horse == nullptr || sActorKeys.contains(&horse->actor)) {
        return;
    }
    EponaKey key{ Session_LocalId(), sequence };
    sActorKeys[&horse->actor] = key;
    EponaState state;
    state.callSequence = sequence;
    state.pos[0] = horse->actor.world.pos.x;
    state.pos[1] = horse->actor.world.pos.y;
    state.pos[2] = horse->actor.world.pos.z;
    state.rot = { horse->actor.shape.rot.x, horse->actor.shape.rot.y, horse->actor.shape.rot.z };
    state.speed = horse->actor.speed;
    state.action = (uint8_t)horse->action;
    state.animation = (uint8_t)horse->animIndex;
    state.ownerMounted = IsMounted(horse);
    state.passengerPresent = false;
    HorseProxyState local;
    local.key = key;
    local.actor = horse;
    local.state = state;
    local.remote = false;
    sHorses[key] = local;
}

void SendCall(PlayState* play, uint32_t sequence, const EnHorse* horse) {
    json ev = MakeEvent(ev::kEponaCall);
    ev["scene"] = play->sceneId;
    ev["horse"] = sequence;
    ev["pos"] = { horse->actor.world.pos.x, horse->actor.world.pos.y, horse->actor.world.pos.z };
    ev["rot"] = { horse->actor.shape.rot.x, horse->actor.shape.rot.y, horse->actor.shape.rot.z };
    NetClient::Get().SendEvent(ev);
}

void OnEponaCall(const json& ev) {
    if (!InGameplay(gPlayState) || GetInt(ev, "scene", -1) != gPlayState->sceneId) {
        return;
    }
    uint8_t owner = (uint8_t)GetInt(ev, "from", 0);
    uint32_t sequence = (uint32_t)GetInt(ev, "horse", 0);
    if (owner == 0 || owner == Session_LocalId() || sequence == 0) {
        return;
    }
    EponaKey key{ owner, sequence };
    if (sHorses.contains(key)) {
        return;
    }
    EponaState state;
    state.callSequence = sequence;
    auto pos = ev.find("pos");
    if (pos != ev.end() && pos->is_array() && pos->size() == 3) {
        state.pos[0] = (*pos)[0].get<float>();
        state.pos[1] = (*pos)[1].get<float>();
        state.pos[2] = (*pos)[2].get<float>();
    } else if (LocalLink() != nullptr) {
        state.pos[0] = LocalLink()->actor.world.pos.x;
        state.pos[1] = LocalLink()->actor.world.pos.y;
        state.pos[2] = LocalLink()->actor.world.pos.z;
    }
    auto rot = ev.find("rot");
    if (rot != ev.end() && rot->is_array() && rot->size() == 3) {
        state.rot = { (int16_t)(*rot)[0].get<int>(), (int16_t)(*rot)[1].get<int>(), (int16_t)(*rot)[2].get<int>() };
    }
    state.ownerMounted = false;
    state.passengerPresent = false;
    HorseProxyState proxy;
    proxy.key = key;
    proxy.state = state;
    sHorses[key] = proxy;
}

void OnEponaState(const uint8_t* data, size_t size) {
    EponaPacket packet;
    if (!DecodeEponaState(data, size, packet) || packet.ownerPlayerId == 0 ||
        packet.ownerPlayerId == Session_LocalId() || Session_FindPlayer(packet.ownerPlayerId) == nullptr ||
        packet.sceneId != (gPlayState == nullptr ? -1 : gPlayState->sceneId)) {
        return;
    }
    std::map<EponaKey, bool> live;
    for (const EponaState& state : packet.horses) {
        EponaKey key{ packet.ownerPlayerId, state.callSequence };
        live[key] = true;
        HorseProxyState& proxy = sHorses[key];
        proxy.key = key;
        bool localPassenger = proxy.localPassenger;
        proxy.state = state;
        proxy.localPassenger = localPassenger;
        proxy.state.passengerPresent = state.passengerPresent || localPassenger;
        proxy.remote = true;
    }
    for (auto it = sHorses.begin(); it != sHorses.end();) {
        if (it->first.ownerPlayerId == packet.ownerPlayerId && it->first.callSequence != 0 && !live.contains(it->first)) {
            if (it->second.actor != nullptr) {
                it->second.pendingDestroy = true;
            } else {
                it = sHorses.erase(it);
                continue;
            }
        }
        ++it;
    }
}

// We sit on it: removing it would leave Link riding freed memory.
bool RiddenByUs(const HorseProxyState& proxy) {
    Player* player = LocalLink();
    return proxy.actor != nullptr && player != nullptr && player->rideActor == &proxy.actor->actor;
}

void DestroyProxy(HorseProxyState& proxy) {
    EnHorse* actor = proxy.actor;
    proxy.actor = nullptr;
    if (actor != nullptr) {
        sActorKeys.erase(&actor->actor);
        if (actor->actor.update != nullptr) {
            Actor_Kill(&actor->actor);
        }
    }
}

void EnsureProxy(PlayState* play, HorseProxyState& proxy) {
    if (!proxy.remote || proxy.actor != nullptr || play == nullptr) {
        return;
    }
    int slot = Object_GetSlot(&play->objectCtx, OBJECT_HORSE_LINK_CHILD);
    if (slot <= OBJECT_SLOT_NONE || !Object_IsLoaded(&play->objectCtx, slot)) {
        return;
    }
    Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, proxy.state.pos[0], proxy.state.pos[1],
                               proxy.state.pos[2], proxy.state.rot.x, proxy.state.rot.y, proxy.state.rot.z,
                               ENHORSE_PARAMS(ENHORSE_PARAM_4000, ENHORSE_2));
    if (actor == nullptr) {
        SPDLOG_WARN("[Coop] Could not spawn proxy Epona ({}, {})", (int)proxy.key.ownerPlayerId,
                    proxy.key.callSequence);
        return;
    }
    proxy.actor = (EnHorse*)actor;
    sActorKeys[actor] = proxy.key;
}

void ApplyProxyState(HorseProxyState& proxy) {
    if (proxy.actor == nullptr || proxy.actor->actor.update == nullptr) {
        return;
    }
    EnHorse* horse = proxy.actor;
    horse->actor.world.pos.x = proxy.state.pos[0];
    horse->actor.world.pos.y = proxy.state.pos[1];
    horse->actor.world.pos.z = proxy.state.pos[2];
    horse->actor.prevPos = horse->actor.world.pos;
    horse->actor.shape.rot.x = proxy.state.rot.x;
    horse->actor.shape.rot.y = proxy.state.rot.y;
    horse->actor.shape.rot.z = proxy.state.rot.z;
    horse->actor.world.rot = horse->actor.shape.rot;
    horse->actor.speed = proxy.state.speed;
    if (horse->stateFlags & ENHORSE_INACTIVE) {
        horse->stateFlags &= ~ENHORSE_INACTIVE;
        horse->action = ENHORSE_ACTION_IDLE;
        horse->animIndex = ENHORSE_ANIM_IDLE;
        horse->colliderCylinder1.base.ocFlags1 |= OC1_ON;
        horse->colliderCylinder2.base.ocFlags1 |= OC1_ON;
        horse->colliderJntSph.base.ocFlags1 |= OC1_ON;
    }
    horse->stateFlags = proxy.state.ownerMounted ? (horse->stateFlags & ~ENHORSE_UNRIDEABLE)
                                                  : (horse->stateFlags | ENHORSE_UNRIDEABLE);
    horse->playerControlled = false;
    horse->curStick.x = 0.0f;
    horse->curStick.z = 0.0f;
    horse->lastStick = horse->curStick;
    if (proxy.state.action <= ENHORSE_ACTION_25 && proxy.state.action != ENHORSE_ACTION_FROZEN) {
        horse->action = (EnHorseAction)proxy.state.action;
    }
    // Its driver got off (or left) while we sit behind: there the horse waits without a rider, an action that would
    // only let our Link spur it ("Faster"). It stands with us on it: the game offers "Down" on a horse standing with
    // its rider (EN_HORSE_CHECK_4), as when the driver is still on.
    if (!proxy.state.ownerMounted && RiddenByUs(proxy)) {
        horse->action = ENHORSE_ACTION_MOUNTED_IDLE;
    }
    // The proxy's action funcs play their own animations locally; forcing animIndex from the stream
    // desynchronizes it from the Skin's curFrame and overruns the rider's paired animation data.
}

void CaptureAndSend(PlayState* play) {
    if (!InGameplay(play) || Session_LocalId() == 0) {
        return;
    }
    EponaPacket packet;
    packet.ownerPlayerId = Session_LocalId();
    packet.seq = ++sPacketSequence;
    packet.sceneId = play->sceneId;
    for (auto& [key, horse] : sHorses) {
        if (key.ownerPlayerId != Session_LocalId() || horse.actor == nullptr || horse.remote ||
            horse.actor->actor.update == nullptr) {
            continue;
        }
        horse.state.callSequence = key.callSequence;
        horse.state.pos[0] = horse.actor->actor.world.pos.x;
        horse.state.pos[1] = horse.actor->actor.world.pos.y;
        horse.state.pos[2] = horse.actor->actor.world.pos.z;
        horse.state.rot = { horse.actor->actor.shape.rot.x, horse.actor->actor.shape.rot.y, horse.actor->actor.shape.rot.z };
        horse.state.speed = horse.actor->actor.speed;
        horse.state.action = (uint8_t)horse.actor->action;
        horse.state.animation = (uint8_t)horse.actor->animIndex;
        horse.state.ownerMounted = IsMounted(horse.actor);
        auto passenger = sPassengers.find(key);
        horse.state.passengerPresent = passenger != sPassengers.end();
        packet.horses.push_back(horse.state);
    }
    if (!SanitizeEponaPacket(packet)) {
        return;
    }
    NetClient::Get().SendStream(EncodeEponaState(packet));
}

bool SameKey(const EponaKey& a, const EponaKey& b) {
    return a.ownerPlayerId == b.ownerPlayerId && a.callSequence == b.callSequence;
}

// Every frame, also while the scene changes: which horse we ride, and never arrive on a horse of our own when we
// were sitting on another player's.
void WatchSceneChange(PlayState* play) {
    Player* player = LocalLink();
    if (play == nullptr || player == nullptr || !WorldSession_Active()) {
        return;
    }
    bool leaving = play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
    Actor* ride = (player->stateFlags1 & PLAYER_STATE1_800000) ? player->rideActor : nullptr;
    if (ride == nullptr) {
        if (!leaving) {
            sOwnRiddenSequence = 0;
        }
        return;
    }
    auto key = sActorKeys.find(ride);
    auto horse = key == sActorKeys.end() ? sHorses.end() : sHorses.find(key->second);
    if (horse == sHorses.end()) {
        return;
    }
    if (!horse->second.remote) {
        sOwnRiddenSequence = key->second.callSequence;
        return;
    }
    if (leaving) {
        gHorseIsMounted = false; // the exit set it: the new scene would give us a horse of our own
        if (sRejoin.untilMs == 0 || !SameKey(sRejoin.key, key->second)) {
            sRejoin = { key->second, NowMs() + kRejoinWaitMs };
        }
    }
}

// A horse of ours the game created without Epona's Song: everyone sees it too.
void Announce(PlayState* play, EnHorse* horse) {
    uint32_t sequence = sNextCallSequence++;
    RegisterOwnerHorse(horse, sequence);
    SendCall(play, sequence, horse);
}

// The driver arrived on the horse the game gave it: it is the one it rode (same key, so its passenger finds it).
// Riding any other horse of ours that nobody knows about (the saved Epona): it is announced.
void AdoptRiddenHorse(PlayState* play) {
    Player* player = LocalLink();
    if (player == nullptr || !(player->stateFlags1 & PLAYER_STATE1_800000) || !IsHorseActor(player->rideActor) ||
        sActorKeys.contains(player->rideActor) || ((EnHorse*)player->rideActor)->type != HORSE_TYPE_2) {
        return;
    }
    if (sOwnRiddenSequence != 0) {
        RegisterOwnerHorse((EnHorse*)player->rideActor, sOwnRiddenSequence);
    } else {
        Announce(play, (EnHorse*)player->rideActor);
    }
}

// The Epona the game brings back where we left her (our saved horse, awake): shown to everyone as ours.
void AdoptSavedHorse(PlayState* play) {
    const HorseData& saved = gSaveContext.save.saveInfo.horseData;
    if (play->sceneId != saved.sceneId) {
        return;
    }
    for (Actor* actor = play->actorCtx.actorLists[ACTORCAT_BG].first; actor != nullptr; actor = actor->next) {
        if (!IsHorseActor(actor) || actor->update == nullptr || sActorKeys.contains(actor)) {
            continue;
        }
        EnHorse* horse = (EnHorse*)actor;
        bool atSavedSpot =
            std::abs(actor->home.pos.x - saved.pos.x) < 50.0f && std::abs(actor->home.pos.z - saved.pos.z) < 50.0f;
        if (horse->type == HORSE_TYPE_2 && horse->actor.params == ENHORSE_1 && atSavedSpot &&
            !(horse->stateFlags & ENHORSE_INACTIVE)) {
            Announce(play, horse);
        }
    }
}

// We arrived on foot after riding behind another player: back on their horse as soon as it is here.
void TryRejoin(PlayState* play) {
    if (sRejoin.untilMs == 0) {
        return;
    }
    if (NowMs() > sRejoin.untilMs) {
        sRejoin = {};
        return;
    }
    Player* player = LocalLink();
    if (player == nullptr || player->rideActor != nullptr || player->transformation != PLAYER_FORM_HUMAN ||
        (player->stateFlags1 & (PLAYER_STATE1_800000 | PLAYER_STATE1_DEAD | PLAYER_STATE1_TALKING |
                                PLAYER_STATE1_CARRYING_ACTOR)) ||
        Play_InCsMode(play)) {
        return;
    }
    auto it = sHorses.find(sRejoin.key);
    if (it == sHorses.end() || !it->second.remote || it->second.actor == nullptr ||
        it->second.actor->actor.update == nullptr || !it->second.state.ownerMounted ||
        (it->second.state.passengerPresent && !it->second.localPassenger)) {
        return;
    }
    Actor* horse = &it->second.actor->actor;
    player->actor.world.pos = horse->world.pos;
    player->actor.prevPos = horse->world.pos;
    player->actor.shape.rot.y = horse->shape.rot.y;
    player->actor.world.rot.y = horse->shape.rot.y;
    player->yaw = horse->shape.rot.y;
    Player_MountHorse(play, player, horse); // Link's own update puts him in the saddle (as on arriving mounted)
    Player_SetCameraHorseSetting(play, player);
    sRejoin = {};
}

void RegisterManager() {
    COND_HOOK(OnGameStateMainStart, true, []() { CoopEpona_FrameStart(gPlayState); });
    COND_HOOK(OnGameStateMainFinish, true, []() { CoopEpona_FrameEnd(gPlayState); });
    COND_HOOK(OnPlayDestroy, true, CoopEpona_OnPlayDestroy);
    COND_HOOK(OnActorDestroy, true, CoopEpona_OnActorDestroy);
}

} // namespace

HorseProxyState* CoopEpona_ProxyState(Actor* actor) {
    auto it = sActorKeys.find(actor);
    if (it == sActorKeys.end()) {
        return nullptr;
    }
    auto horse = sHorses.find(it->second);
    return horse == sHorses.end() || !horse->second.remote ? nullptr : &horse->second;
}

bool CoopEpona_IsProxy(const Actor* actor) {
    return CoopEpona_ProxyState(const_cast<Actor*>(actor)) != nullptr;
}

bool CoopEpona_IsProxyRideable(const Actor* actor) {
    HorseProxyState* state = CoopEpona_ProxyState(const_cast<Actor*>(actor));
    return state != nullptr && state->state.ownerMounted && !state->state.passengerPresent && !state->localPassenger;
}

bool CoopEpona_ProxyHasLocalPassenger(const Actor* actor) {
    HorseProxyState* state = CoopEpona_ProxyState(const_cast<Actor*>(actor));
    return state != nullptr && state->localPassenger;
}

void CoopEpona_FrameStart(PlayState* play) {
    if (!InGameplay(play)) {
        return;
    }
    if (sScene != play->sceneId) {
        for (auto it = sHorses.begin(); it != sHorses.end();) {
            if (it->first.ownerPlayerId != Session_LocalId()) {
                DestroyProxy(it->second);
                it = sHorses.erase(it);
            } else {
                ++it;
            }
        }
        sScene = play->sceneId;
    }
    int slot = Object_GetSlot(&play->objectCtx, OBJECT_HORSE_LINK_CHILD);
    if (slot <= OBJECT_SLOT_NONE) {
        Object_SpawnPersistent(&play->objectCtx, OBJECT_HORSE_LINK_CHILD);
    }
    for (auto& [key, proxy] : sHorses) {
        if (proxy.remote) {
            EnsureProxy(play, proxy);
            ApplyProxyState(proxy);
        }
    }
}

void CoopEpona_FrameEnd(PlayState* play) {
    WatchSceneChange(play);
    if (!InGameplay(play)) {
        return;
    }
    AdoptRiddenHorse(play);
    AdoptSavedHorse(play);
    TryRejoin(play);
    for (auto it = sHorses.begin(); it != sHorses.end();) {
        if (it->second.pendingDestroy && !RiddenByUs(it->second)) { // ridden: when we get off
            DestroyProxy(it->second);
            it = sHorses.erase(it);
        } else {
            ++it;
        }
    }
    Player* player = LocalLink();
    if (player != nullptr) {
        for (auto& [key, proxy] : sHorses) {
            if (!proxy.remote || proxy.actor == nullptr) {
                continue;
            }
            bool mounted = player->rideActor == &proxy.actor->actor && (player->stateFlags1 & PLAYER_STATE1_800000);
            if (mounted != proxy.localPassenger) {
                if (mounted) {
                    sHorseDataBeforeRide = gSaveContext.save.saveInfo.horseData;
                    sHorseDataSaved = true;
                } else if (sHorseDataSaved) {
                    gSaveContext.save.saveInfo.horseData = sHorseDataBeforeRide; // not ours to keep
                    sHorseDataSaved = false;
                }
                proxy.localPassenger = mounted;
                proxy.state.passengerPresent = mounted || proxy.state.passengerPresent;
                json ev = MakeEvent(ev::kEponaPassenger);
                ev["scene"] = play->sceneId;
                ev["owner"] = key.ownerPlayerId;
                ev["horse"] = key.callSequence;
                ev["mounted"] = mounted;
                NetClient::Get().SendEvent(ev);
                if (!mounted) {
                    proxy.state.passengerPresent = false;
                }
            }
        }
    }
    CaptureAndSend(play);
}

void CoopEpona_OnPlayDestroy() {
    for (auto& [key, horse] : sHorses) {
        DestroyProxy(horse);
    }
    sHorses.clear();
    sActorKeys.clear();
    sPassengers.clear();
    sScene = -1;
}

void CoopEpona_OnActorDestroy(Actor* actor) {
    auto key = sActorKeys.find(actor);
    if (key == sActorKeys.end()) {
        for (auto& [horseKey, horse] : sHorses) {
            if (horse.actor != nullptr && &horse.actor->actor == actor) {
                horse.actor = nullptr;
                horse.pendingDestroy = horse.remote;
            }
        }
        return;
    }
    auto horse = sHorses.find(key->second);
    if (horse != sHorses.end()) {
        sPassengers.erase(horse->first);
        horse->second.actor = nullptr;
        horse->second.pendingDestroy = horse->second.remote;
    }
    sActorKeys.erase(key);
}

void CoopEpona_OnSongPlayed(PlayState* play) {
    if (!InGameplay(play) || !Horse_IsValidSpawn(play->sceneId)) {
        return;
    }
    EnHorse* horse = FindAvailableOwnerHorse();
    if (horse == nullptr) {
        Player* player = LocalLink();
        if (player == nullptr) {
            return;
        }
        Actor* actor = Actor_Spawn(&play->actorCtx, play, ACTOR_EN_HORSE, player->actor.world.pos.x,
                                   player->actor.world.pos.y, player->actor.world.pos.z, 0,
                                   player->actor.shape.rot.y, 0, ENHORSE_PARAMS(ENHORSE_PARAM_4000, ENHORSE_11));
        horse = (EnHorse*)actor;
    }
    if (horse == nullptr) {
        SPDLOG_WARN("[Coop] Epona call could not create owner horse");
        return;
    }
    if (horse->stateFlags & ENHORSE_INACTIVE) {
        bool spawned = EnHorse_Spawn(horse, play);
        if (!spawned && LocalLink() != nullptr) {
            horse->actor.world.pos = LocalLink()->actor.world.pos;
            horse->actor.prevPos = horse->actor.world.pos;
            horse->actor.shape.rot.y = LocalLink()->actor.shape.rot.y;
            horse->actor.world.rot.y = horse->actor.shape.rot.y;
        }
        horse->stateFlags &= ~ENHORSE_INACTIVE;
        horse->stateFlags &= ~ENHORSE_UNRIDEABLE;
        horse->action = ENHORSE_ACTION_IDLE;
        horse->animIndex = ENHORSE_ANIM_IDLE;
        horse->colliderCylinder1.base.ocFlags1 |= OC1_ON;
        horse->colliderCylinder2.base.ocFlags1 |= OC1_ON;
        horse->colliderJntSph.base.ocFlags1 |= OC1_ON;
    }
    gHorsePlayedEponasSong = false;
    uint32_t sequence = sNextCallSequence++;
    RegisterOwnerHorse(horse, sequence);
    SendCall(play, sequence, horse);
}

void CoopEpona_ApplyProxy(EnHorse* horse, PlayState*) {
    HorseProxyState* state = CoopEpona_ProxyState(&horse->actor);
    if (state != nullptr) {
        ApplyProxyState(*state);
    }
}

void CoopEpona_BeforeProxyUpdate(EnHorse* horse, PlayState*) {
    HorseProxyState* state = CoopEpona_ProxyState(&horse->actor);
    if (state == nullptr) {
        return;
    }
    horse->playerControlled = false;
    horse->curStick.x = 0.0f;
    horse->curStick.z = 0.0f;
    if (!state->state.ownerMounted) {
        horse->stateFlags |= ENHORSE_UNRIDEABLE;
    }
}

void CoopEpona_AfterProxyUpdate(EnHorse* horse, PlayState*) {
    HorseProxyState* state = CoopEpona_ProxyState(&horse->actor);
    if (state != nullptr) {
        ApplyProxyState(*state);
    }
}

void OnEponaStream(const uint8_t* data, size_t size) {
    OnEponaState(data, size);
}

void OnEponaPassenger(const json& ev) {
    if (!InGameplay(gPlayState) || GetInt(ev, "owner", 0) != Session_LocalId()) {
        return;
    }
    auto mounted = ev.find("mounted");
    if (mounted == ev.end() || !mounted->is_boolean()) {
        return;
    }
    EponaKey key{ Session_LocalId(), (uint32_t)GetInt(ev, "horse", 0) };
    uint8_t from = (uint8_t)GetInt(ev, "from", 0);
    auto horse = sHorses.find(key);
    if (key.callSequence == 0 || from == 0 || from == Session_LocalId() || horse == sHorses.end() || horse->second.remote) {
        return;
    }
    if (mounted->get<bool>()) {
        auto it = sPassengers.find(key);
        if (it == sPassengers.end() || it->second == from) {
            sPassengers[key] = from;
        }
    } else {
        auto it = sPassengers.find(key);
        if (it != sPassengers.end() && it->second == from) {
            sPassengers.erase(it);
        }
    }
}

void ForgetRemoteOwner(uint8_t owner) {
    for (auto it = sHorses.begin(); it != sHorses.end();) {
        if (it->first.ownerPlayerId == owner && it->second.remote && RiddenByUs(it->second)) {
            it->second.pendingDestroy = true; // it stays still until we get off (or follow its owner)
            it->second.state.ownerMounted = false;
            ++it;
        } else if (it->first.ownerPlayerId == owner && it->second.remote) {
            DestroyProxy(it->second);
            it = sHorses.erase(it);
        } else {
            ++it;
        }
    }
}

void OnPlayerLeave(const json& ev) {
    ForgetRemoteOwner((uint8_t)GetInt(ev, "id", 0));
}

// The driver of the horse we sit on went to another scene: we go through the same entrance and get back on there.
bool FollowDriver(uint8_t owner, const json& ev) {
    PlayState* play = gPlayState;
    Player* player = LocalLink();
    int64_t entrance = GetInt(ev, "entrance", -1);
    if (!InGameplay(play) || player == nullptr || entrance < 0 || entrance > 0xFFFF) {
        return false;
    }
    for (auto& [key, proxy] : sHorses) {
        if (key.ownerPlayerId != owner || !proxy.remote || !RiddenByUs(proxy)) {
            continue;
        }
        sRejoin = { key, NowMs() + kRejoinWaitMs };
        gHorseIsMounted = false; // on foot: the game must not bring a horse of our own
        play->nextEntrance = (u16)entrance;
        play->transitionTrigger = TRANS_TRIGGER_START;
        play->transitionType = TRANS_TYPE_FADE_BLACK;
        return true;
    }
    return false;
}

void OnPlayerLocation(const json& ev) {
    uint8_t owner = (uint8_t)GetInt(ev, "id", 0);
    int16_t scene = (int16_t)GetInt(ev, "scene", -1);
    if (owner != 0 && gPlayState != nullptr && scene != gPlayState->sceneId) {
        FollowDriver(owner, ev);
        ForgetRemoteOwner(owner);
    }
    for (auto it = sPassengers.begin(); it != sPassengers.end();) {
        if (it->second == owner) {
            it = sPassengers.erase(it);
        } else {
            ++it;
        }
    }
}

void OnLost(const std::string&) {
    CoopEpona_OnPlayDestroy();
    sNextCallSequence = 1;
    sOwnRiddenSequence = 0;
    sRejoin = {};
    sHorseDataSaved = false;
}

COOP_ON_EVENT(eponaManagerCall, ev::kEponaCall, OnEponaCall);
COOP_ON_EVENT(eponaManagerPassenger, ev::kEponaPassenger, OnEponaPassenger);
COOP_ON_EVENT(eponaManagerLeave, ev::kLeave, OnPlayerLeave);
COOP_ON_EVENT(eponaManagerLoc, ev::kLoc, OnPlayerLocation);
COOP_ON_STREAM(eponaManagerState, kStreamEponaState, OnEponaStream);
COOP_ON_LOST(eponaManagerLost, OnLost);
static RegisterShipInitFunc sEponaManagerInit(RegisterManager);

} // namespace coop::client

extern "C" {
void Coop_EponaSongPlayed(PlayState* play) { coop::client::CoopEpona_OnSongPlayed(play); }
void Coop_EponaFrameStart(PlayState* play) { coop::client::CoopEpona_FrameStart(play); }
void Coop_EponaFrameEnd(PlayState* play) { coop::client::CoopEpona_FrameEnd(play); }
void Coop_EponaOnPlayDestroy() { coop::client::CoopEpona_OnPlayDestroy(); }
void Coop_EponaOnActorDestroy(Actor* actor) { coop::client::CoopEpona_OnActorDestroy(actor); }
s32 Coop_EponaIsProxy(const Actor* actor) { return coop::client::CoopEpona_IsProxy(actor) ? 1 : 0; }
s32 Coop_EponaIsProxyRideable(const Actor* actor) { return coop::client::CoopEpona_IsProxyRideable(actor) ? 1 : 0; }
s32 Coop_EponaProxyHasLocalPassenger(const Actor* actor) {
    return coop::client::CoopEpona_ProxyHasLocalPassenger(actor) ? 1 : 0;
}
void Coop_EponaBeforeProxyUpdate(EnHorse* horse, PlayState* play) {
    coop::client::CoopEpona_BeforeProxyUpdate(horse, play);
}
void Coop_EponaAfterProxyUpdate(EnHorse* horse, PlayState* play) {
    coop::client::CoopEpona_AfterProxyUpdate(horse, play);
}
}

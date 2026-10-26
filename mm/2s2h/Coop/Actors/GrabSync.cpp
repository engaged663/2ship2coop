// [COOP] Grabs (spec docs/superpowers/specs/2026-10-26-coop-agarres-design.md). A shared enemy chasing another
// player's puppet sees it as Link (GET_PLAYER is the puppet while it updates): Gyorg's bite, ReDeads, Gibdos, Like
// Likes, Floormasters, Dexihands, Majora's whip... grab it with play->grabPlayer.
//  - The grabber's game: "grab" "start" to that player; after each update of the grabber, what it did to the puppet:
//    moved it ("hold": place, and whether it carries it as its child), let go ("free": the actionVar2 it set), or
//    nothing ("hold" without a place every kKeepAliveMs). Until that player's pose shows its Link held
//    (PLAYER_STATE2_80), the puppet keeps that bit: the ReDead would think its prey escaped.
//  - The grabbed player's game: its own Link is grabbed the engine's way (Player_GrabPlayer: the same grabbed action,
//    mashing buttons included) and placed where the enemy puts the puppet, after its own update, as the enemy would.
// Damage while held travels as "hurt" "pain" (HitSync.cpp). Switch: gCoop.Sync.Grabs / server.json "grabs".
#include "GrabSync.h"

#include "ActorRegistry.h"
#include "ActorSync.h"
#include "CoopEngine.h"
#include "Leases.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Puppet/PuppetActor.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/Sync/Sync.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include "common/Protocol.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <map>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kConfirmMs = 1500;     // the victim's pose shows its Link held by then, or it could not be grabbed
constexpr int64_t kKeepAliveMs = 500;    // a "hold" at least this often while an enemy holds without moving its prey
constexpr int64_t kLooseMs = 1000;       // the victim: no news this long and its Link no longer held: over
constexpr int64_t kLostMs = 3000;        // the victim: no news this long: over, held or not
constexpr size_t kMaxIncoming = 32;
constexpr u16 kVictimTexts[] = { 0xF6 }; // what a grabber may show on its victim's screen (a Like Like ate your shield)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool On() {
    return gPlayState != nullptr && Sync_On(SyncPart::Grabs);
}

Player* LocalLink() {
    return (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool SamePos(const Vec3f& a, const Vec3f& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameRot(const Vec3s& a, const Vec3s& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

json Op(uint8_t to, const char* op) {
    json ev = MakeEvent(ev::kGrab);
    ev["to"] = to;
    ev["op"] = op;
    return ev;
}

void Place(json& ev, const Actor& actor) {
    ev["pos"] = { actor.world.pos.x, actor.world.pos.y, actor.world.pos.z };
    ev["rot"] = { actor.shape.rot.x, actor.shape.rot.y, actor.shape.rot.z };
}

// ---- The grabber's game: what an enemy of ours holds, by the grabbed player's id ----

struct Hold {
    Player* puppet = nullptr;
    Actor* grabber = nullptr;
    int64_t startMs = 0;
    int64_t sentMs = 0;
    bool confirmed = false; // its player's pose showed its Link held
    // The puppet before the grabber's update
    bool snapped = false;
    Vec3f pos = {};
    Vec3s rot = {};
    u32 flags2 = 0;
    s16 av2 = 0;
    Actor* parent = nullptr;
};

std::map<uint8_t, Hold> sHolds;
Actor* sLocalGrabber = nullptr; // what grabbed our own Link

// The puppet is still that player's body here.
bool PuppetValid(uint8_t id, const Hold& h) {
    Actor* puppet = PuppetManager_Actor(id);
    return puppet != nullptr && puppet == &h.puppet->actor && puppet->update != nullptr;
}

void Send(json ev) {
    NetClient::Get().SendEvent(ev);
}

// ---- The grabbed player's game ----

struct Grabbed {
    bool active = false;
    uint8_t from = 0;
    uint32_t key = 0;
    int16_t actorId = 0;
    int64_t lastMs = 0;
    bool placed = false; // a place arrived: our Link goes there after each of its updates
    Vec3f pos = {};
    Vec3s rot = {};
    bool attached = false;
    Actor* parentSet = nullptr; // the copy we made our Link's parent
    bool blurring = false;
};

Grabbed sGrabbed;
std::deque<json> sIncoming; // for our Link, before its next update

// Our copy of the enemy that holds us (nullptr if it is not here).
Actor* GrabberCopy() {
    TrackedActor* t = ActorRegistry_Find(sGrabbed.key);
    if (t == nullptr || t->actor == nullptr || t->actor->update == nullptr || t->actor->id != sGrabbed.actorId) {
        return nullptr;
    }
    return t->actor;
}

void EndGrabbed(Player* link, bool release) {
    if (link != nullptr) {
        if (release && (link->stateFlags2 & PLAYER_STATE2_80) && link->av2.actionVar2 < 100) {
            link->av2.actionVar2 = 100; // its grabbed action lets go on its next update
        }
        if (sGrabbed.parentSet != nullptr && link->actor.parent == sGrabbed.parentSet) {
            link->actor.parent = nullptr;
        }
    }
    if (sGrabbed.blurring) {
        Play_DisableMotionBlur();
    }
    sGrabbed = {};
}

void ShowText(u16 id) {
    if (std::find(std::begin(kVictimTexts), std::end(kVictimTexts), id) != std::end(kVictimTexts) &&
        Message_GetState(&gPlayState->msgCtx) == TEXT_STATE_NONE) {
        Message_StartTextbox(gPlayState, id, nullptr);
    }
}

void Apply(Player* link, const json& ev) {
    std::string op = GetString(ev, "op");
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (op == "text") {
        ShowText((u16)GetInt(ev, "id"));
        return;
    }
    if (op == "start") {
        if (sGrabbed.active) {
            return; // already held (and so Player_GrabPlayer would refuse)
        }
        if (gPlayState->grabPlayer == nullptr || !gPlayState->grabPlayer(gPlayState, link)) {
            SPDLOG_INFO("[Coop] An enemy of player {} grabbed our Link, but it cannot be grabbed now", (int)from);
            return;
        }
        sGrabbed.active = true;
        sGrabbed.from = from;
        sGrabbed.key = (uint32_t)GetInt(ev, "key");
        sGrabbed.actorId = (int16_t)GetInt(ev, "actorId");
        sGrabbed.lastMs = NowMs();
        SPDLOG_INFO("[Coop] Enemy {:#x} of player {} grabbed our Link", (uint16_t)sGrabbed.actorId, (int)from);
        return;
    }
    if (!sGrabbed.active || from != sGrabbed.from) {
        return;
    }
    sGrabbed.lastMs = NowMs();
    auto pos = ev.find("pos");
    auto rot = ev.find("rot");
    if (pos != ev.end() && rot != ev.end() && pos->size() == 3 && rot->size() == 3) { // the server checked them
        sGrabbed.placed = true;
        sGrabbed.pos = { (*pos)[0].get<f32>(), (*pos)[1].get<f32>(), (*pos)[2].get<f32>() };
        sGrabbed.rot = { (s16)(*rot)[0].get<int>(), (s16)(*rot)[1].get<int>(), (s16)(*rot)[2].get<int>() };
        sGrabbed.attached = GetBool(ev, "attached");
    }
    if (op == "free") {
        if (sGrabbed.placed) {
            link->actor.world.pos = sGrabbed.pos; // where it let go (a Like Like spits)
            link->actor.shape.rot = link->actor.world.rot = sGrabbed.rot;
        }
        if (link->stateFlags2 & PLAYER_STATE2_80) {
            link->av2.actionVar2 = (s16)std::clamp<int64_t>(GetInt(ev, "av2"), 100, 255);
        }
        SPDLOG_INFO("[Coop] The enemy of player {} let go of our Link", (int)from);
        EndGrabbed(link, false);
    }
}

// Before our Link updates: what arrived for it.
void OnShouldLinkUpdate(Actor* actor, bool* should) {
    if (gPlayState == nullptr || actor != &LocalLink()->actor) {
        return;
    }
    while (!sIncoming.empty()) {
        json ev = std::move(sIncoming.front());
        sIncoming.pop_front();
        if (On()) {
            Apply((Player*)actor, ev);
        }
    }
}

// After our Link updates: where the enemy holds it (as the enemy itself would, updating after Link).
void AfterLinkUpdate(Actor* actor) {
    if (!sGrabbed.active || gPlayState == nullptr || actor != &LocalLink()->actor) {
        return;
    }
    Player* link = (Player*)actor;
    TrackedActor* t = ActorRegistry_Find(sGrabbed.key);
    if (t != nullptr && t->actor != nullptr && Leases_IsMine(*t)) {
        if (sGrabbed.blurring) {
            Play_DisableMotionBlur();
        }
        sGrabbed = {}; // the enemy is ours now: it goes on with our Link itself (holding it, letting go)
        return;
    }
    int64_t idle = NowMs() - sGrabbed.lastMs;
    if (!On() || idle > kLostMs || (idle > kLooseMs && !(link->stateFlags2 & PLAYER_STATE2_80))) {
        EndGrabbed(link, true);
        return;
    }
    if (!sGrabbed.placed) {
        return;
    }
    link->actor.world.pos = sGrabbed.pos;
    link->actor.shape.rot = link->actor.world.rot = sGrabbed.rot;
    Actor* copy = sGrabbed.attached ? GrabberCopy() : nullptr;
    if (copy != nullptr) {
        link->actor.parent = copy;
        sGrabbed.parentSet = copy;
    } else if (sGrabbed.parentSet != nullptr && link->actor.parent == sGrabbed.parentSet) {
        link->actor.parent = nullptr;
        sGrabbed.parentSet = nullptr;
    }
    if (sGrabbed.attached && sGrabbed.actorId == ACTOR_BOSS_03 && !sGrabbed.blurring) {
        Play_EnableMotionBlur(150); // Gyorg chewing: what its own game no longer shows (z_boss_03.c)
        sGrabbed.blurring = true;
    }
}

void OnGrabEvent(const json& ev) {
    if (gPlayState == nullptr) {
        return;
    }
    // Only the last of several holds in a row matters (our Link may not update for a while: the pause menu)
    if (GetString(ev, "op") == "hold" && !sIncoming.empty() && GetString(sIncoming.back(), "op") == "hold") {
        if (ev.find("pos") != ev.end() || sIncoming.back().find("pos") == sIncoming.back().end()) {
            sIncoming.back() = ev;
        }
        return;
    }
    if (sIncoming.size() >= kMaxIncoming) {
        sIncoming.pop_front();
    }
    sIncoming.push_back(ev);
}

// Once a frame (the grabber's game): holds whose puppet or grabber went away, keep-alives, unconfirmed grabs.
void Tick() {
    int64_t now = NowMs();
    for (auto it = sHolds.begin(); it != sHolds.end();) {
        uint8_t id = it->first;
        Hold& h = it->second;
        TrackedActor* t = ActorRegistry_Get(h.grabber);
        bool attached = PuppetValid(id, h) && h.puppet->actor.parent == h.grabber;
        bool unconfirmed = !h.confirmed && now - h.startMs > kConfirmMs && !attached;
        if (!On() || !PuppetValid(id, h) || t == nullptr || !Leases_IsMine(*t) || unconfirmed) {
            it = sHolds.erase(it); // its new simulator goes on with it; an unconfirmed grab was refused there
            continue;
        }
        if (now - h.sentMs >= kKeepAliveMs) {
            Send(Op(id, "hold"));
            h.sentMs = now;
        }
        ++it;
    }
}

void Clear() {
    if (sGrabbed.blurring) {
        Play_DisableMotionBlur();
    }
    sGrabbed = {};
    sHolds.clear();
    sIncoming.clear();
    sLocalGrabber = nullptr;
}

} // namespace

void GrabSync_UpdateBegin(Actor* actor) {
    for (auto& [id, h] : sHolds) {
        if (h.grabber != actor || !PuppetValid(id, h)) {
            continue;
        }
        h.snapped = true;
        h.pos = h.puppet->actor.world.pos;
        h.rot = h.puppet->actor.shape.rot;
        h.flags2 = h.puppet->stateFlags2;
        h.av2 = h.puppet->av2.actionVar2;
        h.parent = h.puppet->actor.parent;
    }
}

void GrabSync_UpdateEnd(Actor* actor) {
    for (auto it = sHolds.begin(); it != sHolds.end();) {
        uint8_t id = it->first;
        Hold& h = it->second;
        if (h.grabber != actor || !PuppetValid(id, h)) {
            ++it;
            continue;
        }
        Player* p = h.puppet;
        bool attached = p->actor.parent == actor;
        // The update that grabbed it has no "before": what it carries goes at once
        bool moved = h.snapped ? (!SamePos(p->actor.world.pos, h.pos) || !SameRot(p->actor.shape.rot, h.rot)) : attached;
        bool released = h.snapped && ((p->av2.actionVar2 >= 100 && h.av2 < 100) ||
                                      ((h.flags2 & PLAYER_STATE2_80) && !(p->stateFlags2 & PLAYER_STATE2_80)) ||
                                      (h.parent == actor && !attached));
        bool escaped = h.snapped && h.confirmed && !(h.flags2 & PLAYER_STATE2_80) && !attached;
        h.snapped = false;
        if (released) {
            json ev = Op(id, "free");
            ev["av2"] = std::clamp<int>(p->av2.actionVar2, 100, 255);
            if (moved) {
                Place(ev, p->actor);
            }
            Send(std::move(ev));
            SPDLOG_INFO("[Coop] Enemy {:#x} let go of player {}", (uint16_t)actor->id, (int)id);
            it = sHolds.erase(it);
            continue;
        }
        if (escaped) {
            it = sHolds.erase(it); // its player got free there (mashing buttons): the enemy saw it in the pose
            continue;
        }
        if (moved) {
            json ev = Op(id, "hold");
            Place(ev, p->actor);
            ev["attached"] = attached;
            Send(std::move(ev));
            h.sentMs = NowMs();
        }
        ++it;
    }
}

Player* GrabSync_Prey(Actor* actor) {
    for (auto& [id, h] : sHolds) {
        if (h.grabber == actor && PuppetValid(id, h)) {
            return h.puppet;
        }
    }
    if (sLocalGrabber == actor && gPlayState != nullptr) {
        Player* link = LocalLink();
        if (link->stateFlags2 & PLAYER_STATE2_80) {
            return link;
        }
        sLocalGrabber = nullptr;
    }
    return nullptr;
}

void GrabSync_OnActorGone(const Actor* actor) {
    for (auto it = sHolds.begin(); it != sHolds.end();) {
        if (it->second.grabber == actor) {
            if (On() && PuppetValid(it->first, it->second)) {
                json ev = Op(it->first, "free");
                ev["av2"] = 100;
                Send(std::move(ev)); // gone while it held it (killed, unloaded)
            }
            it = sHolds.erase(it);
        } else {
            ++it;
        }
    }
    if (sLocalGrabber == actor) {
        sLocalGrabber = nullptr;
    }
}

uint32_t GrabSync_PuppetFlags2(uint8_t playerId, uint32_t poseFlags2) {
    auto it = sHolds.find(playerId);
    if (it == sHolds.end()) {
        return 0;
    }
    Hold& h = it->second;
    if (poseFlags2 & PLAYER_STATE2_80) {
        h.confirmed = true;
    }
    return (!h.confirmed && NowMs() - h.startMs < kConfirmMs) ? PLAYER_STATE2_80 : 0;
}

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnPlayerGrabbed(PlayState* play, Player* player) {
    if (!On()) {
        return;
    }
    if (player == LocalLink()) {
        Actor* by = ActorSync_AnyUpdating(); // it keeps chasing our Link while it holds it
        sLocalGrabber = by != &player->actor ? by : nullptr; // (not a grab from another game: Apply)
        return;
    }
    TrackedActor* t = ActorSync_Updating();
    if (player->actor.id != ACTOR_EN_COOP_PUPPET || t == nullptr || t->actor == nullptr) {
        return;
    }
    uint8_t id = (uint8_t)COOP_PUPPET_GET_PLAYER_ID(&player->actor);
    Hold h;
    h.puppet = player;
    h.grabber = t->actor;
    h.startMs = h.sentMs = NowMs();
    sHolds[id] = h;
    json ev = Op(id, "start");
    ev["key"] = t->key;
    ev["actorId"] = (uint16_t)t->actor->id;
    Send(std::move(ev));
    SPDLOG_INFO("[Coop] Enemy {:#x} grabbed player {}", (uint16_t)t->actor->id, (int)id);
}

extern "C" s32 Coop_GrabVictimText(PlayState* play, Player* player, u16 textId) {
    if (player == nullptr || player->actor.id != ACTOR_EN_COOP_PUPPET) {
        return false;
    }
    if (On()) {
        json ev = Op((uint8_t)COOP_PUPPET_GET_PLAYER_ID(&player->actor), "text");
        ev["id"] = textId;
        Send(std::move(ev));
    }
    return true; // about another player's Link: never on our screen
}

static void RegisterGrabSync() {
    COND_ID_HOOK(ShouldActorUpdate, ACTOR_PLAYER, true, OnShouldLinkUpdate);
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, AfterLinkUpdate);
    COND_HOOK(OnGameStateMainFinish, true, []() {
        if (gPlayState != nullptr) {
            Tick();
        }
    });
    COND_HOOK(OnPlayDestroy, true, Clear);
}

COOP_ON_EVENT(grabEvent, coop::ev::kGrab, OnGrabEvent);
COOP_ON_LOST(grabLost, [](const std::string&) { Clear(); });
static RegisterShipInitFunc sGrabSyncInit(RegisterGrabSync);

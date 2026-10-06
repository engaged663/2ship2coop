// [COOP] See Leases.h. Distances: ask at < 400 units, give back at > 600 when not talking (the server keeps it
// while we talk, and moves it to another player only if they are 25 % closer).
#include "Leases.h"

#include "Authority.h"
#include "ReplicationRules.h"

#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/Sync/Sync.h"
#include "2s2h/Coop/World/WorldSession.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <utility>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr float kAskDist = 400.f;
constexpr float kGiveBackDist = 600.f;
constexpr int kRenewFrames = 10; // twice a second at 20 frames per second
// After a cutscene of one of ours its family stays ours this long: the next one, or its talk, comes right after
// (Bomber Jim: his balloon's cutscene, then his own).
constexpr int64_t kCutsceneGraceMs = 1500;

int16_t sScene = -1;
std::map<std::pair<int8_t, uint32_t>, uint8_t> sHolders;
int sFrame = 0;

// The family whose cutscene runs here (Leases_HoldsCutscene).
struct CutscenePin {
    int16_t scene = -1;
    int8_t room = -1;
    uint32_t root = 0;
    int64_t untilMs = 0;
};
CutscenePin sPin;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void OnLeases(const json& ev) {
    std::map<std::pair<int8_t, uint32_t>, uint8_t> holders;
    auto list = ev.find("list");
    if (list != ev.end() && list->is_array()) {
        for (const json& l : *list) {
            if (l.is_array() && l.size() == 3 && l[0].is_number_integer() && l[1].is_number_integer() &&
                l[2].is_number_integer()) {
                holders[{ (int8_t)l[0].get<int>(), l[1].get<uint32_t>() }] = (uint8_t)l[2].get<int>();
            }
        }
    }
    sScene = (int16_t)GetInt(ev, "scene", -1);
    sHolders = std::move(holders);
}

bool Known() {
    return gPlayState != nullptr && sScene == gPlayState->sceneId;
}

// Our Link is talking to someone of this family.
bool Talking(const TrackedActor& root) {
    Player* link = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (link == nullptr || link->talkActor == nullptr || gPlayState->msgCtx.msgMode == MSGMODE_NONE) {
        return false;
    }
    TrackedActor* t = ActorRegistry_Get(link->talkActor);
    return t != nullptr && t->rootKey == root.key && t->room == root.room;
}

// The shared actor of ours whose cutscene this game shows now (nullptr: none).
TrackedActor* CutsceneOfOurs() {
    Actor* a = Cinema_CutsceneActor();
    TrackedActor* t = a != nullptr ? ActorRegistry_Get(a) : nullptr;
    return (t != nullptr && t->owner == 0 && !t->cinema && Leases_IsMine(*t)) ? t : nullptr;
}

// Every frame: a cutscene of one of ours holds its family, asked for at once as talking (nobody takes it meanwhile).
// Without this it changed hands in the middle and the cutscene ran forever in both games (Bomber Jim).
void UpdatePin(Player* link) {
    TrackedActor* t = CutsceneOfOurs();
    if (t == nullptr || !IsListKey(t->rootKey)) {
        return; // (a family made at run time is not lent: the room stays ours, Location.cpp)
    }
    int64_t now = NowMs();
    bool fresh = sPin.scene != gPlayState->sceneId || sPin.room != t->room || sPin.root != t->rootKey ||
                 now >= sPin.untilMs;
    sPin = { gPlayState->sceneId, t->room, t->rootKey, now + kCutsceneGraceMs };
    if (!fresh) {
        return;
    }
    TrackedActor* root = ActorRegistry_Find(t->rootKey);
    json ev = MakeEvent(ev::kLeaseReq);
    ev["scene"] = gPlayState->sceneId;
    ev["room"] = t->room;
    ev["key"] = t->rootKey;
    ev["dist"] = root != nullptr ? std::min(Actor_WorldDistXYZToActor(&link->actor, root->actor), 99999.f) : 0.f;
    ev["talking"] = true;
    NetClient::Get().SendEvent(ev);
}

bool Pinned(const TrackedActor& t) {
    return sPin.scene == gPlayState->sceneId && t.room == sPin.room && t.key == sPin.root && NowMs() < sPin.untilMs;
}

} // namespace

uint8_t Leases_Holder(int8_t room, uint32_t rootKey) {
    if (!Known()) {
        return 0;
    }
    auto it = sHolders.find({ room, rootKey });
    return it == sHolders.end() ? 0 : it->second;
}

uint8_t Leases_Owner(const TrackedActor& t) {
    if (t.owner != 0) {
        return t.owner; // a player's own object (Sync/PlayerObjects.cpp)
    }
    if (t.cinema) {
        return Cinema_ActorOwner(); // a cutscene actor: ours, or the game's whose cutscene we watch
    }
    uint8_t lessee = Leases_Holder(t.room, t.rootKey);
    return lessee != 0 ? lessee : Authority_Owner(t.room);
}

bool Leases_IsMine(const TrackedActor& t) {
    uint8_t owner = Leases_Owner(t);
    return owner != 0 && owner == Session_LocalId();
}

bool Leases_IsRemote(const TrackedActor& t) {
    uint8_t owner = Leases_Owner(t);
    return owner != 0 && owner != Session_LocalId();
}

bool Leases_LentToMe(const TrackedActor& t) {
    uint8_t lessee = Leases_Holder(t.room, t.rootKey);
    return lessee != 0 && lessee == Session_LocalId();
}

bool Leases_HoldAny() {
    if (!Known()) {
        return false;
    }
    for (const auto& [k, id] : sHolders) {
        if (id == Session_LocalId()) {
            return true;
        }
    }
    return false;
}

bool Leases_HoldsCutscene() {
    if (gPlayState == nullptr) {
        return false;
    }
    return CutsceneOfOurs() != nullptr || (sPin.scene == gPlayState->sceneId && NowMs() < sPin.untilMs);
}

void Leases_Tick() {
    if (HostMode_Enabled() || !WorldSession_Active() || EndingMode_Active() || gPlayState == nullptr ||
        !Authority_Known()) {
        return; // the server's own games never borrow
    }
    Player* link = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (link == nullptr) {
        return;
    }
    UpdatePin(link);
    if (++sFrame % 5 != 0) {
        return;
    }
    bool renew = sFrame % kRenewFrames == 0;
    for (TrackedActor* t : ActorRegistry_All()) {
        // (A prop that goes with an NPC, ReplicationRules.cpp kPartners, is lent with it: its family's root)
        if (!IsListKey(t->key) || t->actor->update == nullptr || t->cinema || t->rootKey != t->key) {
            continue;
        }
        // A minigame we run keeps its NPC and props ours (nobody closer takes them); the ones of a mate's are never
        // asked for. A family whose cutscene runs here stays ours until it ends. Otherwise: NPC, enemy or prop in
        // reach (a chicken is a prop, a dog an enemy).
        bool pinned = Director_PinsActorId(t->actor->id, gPlayState->sceneId) || Pinned(*t);
        uint8_t cat = t->actor->category;
        // The scene's machinery (Sync/SceneObjects.cpp): only who touches it (or comes within reach) asks for it, and
        // keeps it while touching; then it goes back to the room's owner
        TouchPolicy policy = SceneObjects_Policy(t->actor->id);
        if (!pinned && policy != TouchPolicy::None) {
            float nearDist = Actor_WorldDistXYZToActor(&link->actor, t->actor);
            bool touched = SceneObjects_Touched(t->actor, nearDist);
            uint8_t has = Leases_Holder(t->room, t->key);
            bool own = has != 0 && has == Session_LocalId();
            if (touched && (!own || renew)) {
                json ev = MakeEvent(ev::kLeaseReq);
                ev["scene"] = gPlayState->sceneId;
                ev["room"] = t->room;
                ev["key"] = t->key;
                ev["dist"] = std::min(nearDist, 99999.f);
                ev["talking"] = true;
                NetClient::Get().SendEvent(ev);
            } else if (!touched && own) {
                json ev = MakeEvent(ev::kLeaseDrop);
                ev["scene"] = gPlayState->sceneId;
                ev["room"] = t->room;
                ev["key"] = t->key;
                NetClient::Get().SendEvent(ev);
            }
            continue;
        }
        if (!pinned && cat != ACTORCAT_NPC && cat != ACTORCAT_ENEMY && cat != ACTORCAT_PROP) {
            continue;
        }
        float dist = Actor_WorldDistXYZToActor(&link->actor, t->actor);
        bool talking = Talking(*t) || pinned;
        uint8_t holder = Leases_Holder(t->room, t->key);
        bool mine = holder != 0 && holder == Session_LocalId();
        // The NPC and props of the minigame a mate runs here are its game's: never asked for, given back if held
        bool theirs = Guest_SkipsLease(t->actor->id);
        if (theirs && !mine) {
            continue;
        }
        if (mine && (theirs || (dist > kGiveBackDist && !talking))) {
            json ev = MakeEvent(ev::kLeaseDrop);
            ev["scene"] = gPlayState->sceneId;
            ev["room"] = t->room;
            ev["key"] = t->key;
            NetClient::Get().SendEvent(ev);
        } else if ((mine && renew) || (!mine && (dist < kAskDist || pinned) && (renew || holder == 0))) {
            json ev = MakeEvent(ev::kLeaseReq);
            ev["scene"] = gPlayState->sceneId;
            ev["room"] = t->room;
            ev["key"] = t->key;
            ev["dist"] = std::min(dist, 99999.f);
            ev["talking"] = talking;
            NetClient::Get().SendEvent(ev);
        }
    }
}

} // namespace coop::client

COOP_ON_EVENT(leasesTable, coop::ev::kLeases, coop::client::OnLeases);
COOP_ON_LOST(leasesLost, [](const std::string&) {
    coop::client::sHolders.clear();
    coop::client::sScene = -1;
    coop::client::sPin = {};
});

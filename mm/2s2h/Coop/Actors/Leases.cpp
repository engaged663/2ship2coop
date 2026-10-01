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
#include "2s2h/Coop/World/WorldSession.h"

#include <algorithm>
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

int16_t sScene = -1;
std::map<std::pair<int8_t, uint32_t>, uint8_t> sHolders;
int sFrame = 0;

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

} // namespace

uint8_t Leases_Holder(int8_t room, uint32_t rootKey) {
    if (!Known()) {
        return 0;
    }
    auto it = sHolders.find({ room, rootKey });
    return it == sHolders.end() ? 0 : it->second;
}

uint8_t Leases_Owner(const TrackedActor& t) {
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

void Leases_Tick() {
    if (HostMode_Enabled() || !WorldSession_Active() || EndingMode_Active() || gPlayState == nullptr ||
        !Authority_Known() ||
        ++sFrame % 5 != 0) {
        return; // the server's own games never borrow
    }
    Player* link = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    if (link == nullptr) {
        return;
    }
    bool renew = sFrame % kRenewFrames == 0;
    for (TrackedActor* t : ActorRegistry_All()) {
        if (!IsListKey(t->key) || t->actor->update == nullptr || t->cinema) {
            continue;
        }
        // A minigame we run keeps its NPC and props ours (nobody closer takes them); the ones of a mate's are never
        // asked for. Otherwise: NPC, enemy or prop in reach (a chicken is a prop, a dog an enemy).
        bool pinned = Director_PinsActorId(t->actor->id, gPlayState->sceneId);
        uint8_t cat = t->actor->category;
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
});

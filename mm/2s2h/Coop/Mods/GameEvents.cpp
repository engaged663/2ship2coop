// [COOP] What happens in this game, for the server's mods (coop/server/Handlers/ModHandlers.cpp checks it again):
//   gev   k = item (id: Item_Give, but not the server's own orders), death (Link started dying), boss (actor: a boss
//         was defeated), kill (actor, params, by, room, pos: a killing blow landed on an enemy this game simulates)
//   stat  hp, hpMax, mp, rupees: when they change, 4 a second at most, and once on entering the world
// Only while playing in the server's world with gCoop.Mods on.
#include "Mods.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/PlayerState.h"
#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kStatEveryMs = 1000 / kStatPerSecond; // 250 ms
constexpr int64_t kRecentHitMs = 3000; // a kill is the remote player's whose hit was applied this recently

bool sWasDying = false;
json sLastStat; // null: none sent since entering the world
int64_t sLastStatMs = 0;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool Reporting() {
    return Mods_Enabled() && Session_IsConnected() && WorldSession_Active();
}

void Report(const char* kind, json fields = json::object()) {
    json ev = MakeEvent(ev::kGameEvent);
    ev["k"] = kind;
    ev.update(fields);
    NetClient::Get().SendEvent(ev);
}

void OnItemGive(u8 item) {
    if (Reporting() && !Mods_GivingItem()) {
        Report("item", { { "id", item } });
    }
}

void OnBossDefeated(s16 actorId) {
    if (Reporting()) {
        Report("boss", { { "actor", actorId } });
    }
}

// End of every frame: Link started dying, and his numbers.
void FrameEnd() {
    if (!Reporting() || !PoseCapture_InGameplay()) {
        sWasDying = false;
        sLastStat = nullptr; // the first ones go again on entering
        return;
    }
    bool dying = GET_PLAYER(gPlayState)->stateFlags1 & PLAYER_STATE1_DEAD;
    if (dying && !sWasDying) {
        Report("death");
    }
    sWasDying = dying;
    const SavePlayerData& p = gSaveContext.save.saveInfo.playerData;
    json stat = { { "hp", std::clamp<int>(p.health, 0, kMaxModHealth) },
                  { "hpMax", std::clamp<int>(p.healthCapacity, 0, kMaxModHealth) },
                  { "mp", std::clamp<int>(p.magic, 0, 255) },
                  { "rupees", std::clamp<int>(p.rupees + gSaveContext.rupeeAccumulator, 0, 99999) } }; // where it goes
    int64_t now = NowMs();
    if (stat != sLastStat && (sLastStat.is_null() || now - sLastStatMs >= kStatEveryMs)) {
        sLastStat = stat;
        sLastStatMs = now;
        json ev = MakeEvent(ev::kStat);
        ev.update(stat);
        NetClient::Get().SendEvent(ev);
    }
}

void RegisterGameEvents() {
    COND_HOOK(OnItemGive, true, OnItemGive);
    COND_HOOK(OnBossDefeated, true, OnBossDefeated);
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

} // namespace coop::client

using namespace coop;
using namespace coop::client;

extern "C" void Coop_OnEnemyDefeated(PlayState* play, Actor* actor) {
    if (play == nullptr || actor == nullptr || !Reporting()) {
        return;
    }
    const Vec3f& pos = actor->world.pos;
    for (float v : { pos.x, pos.y, pos.z }) {
        if (!(std::fabs(v) <= pose_limits::kWorldLimit)) {
            return; // the server would refuse it
        }
    }
    // Who gave the blow: the remote player whose hit we applied to our copy just now, or this game's Link.
    uint8_t by = Session_LocalId();
    TrackedActor* t = ActorRegistry_Get(actor);
    if (t != nullptr && t->lastHitFrom != 0 && NowMs() - t->lastHitMs < kRecentHitMs) {
        by = t->lastHitFrom;
    }
    Report("kill", { { "actor", actor->id },
                     { "params", actor->params },
                     { "by", by },
                     { "room", actor->room },
                     { "pos", { pos.x, pos.y, pos.z } } });
}

static RegisterShipInitFunc sGameEventsInit(coop::client::RegisterGameEvents);

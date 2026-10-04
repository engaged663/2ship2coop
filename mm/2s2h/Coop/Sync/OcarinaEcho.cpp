// [COOP] Sincronización total S6 (spec §7): the notes another player's ocarina plays (in their pose) sound here on the
// ocarina channel, at their puppet, while our own ocarina is quiet: the nearest one playing within kHearDist.
// gCoop.Sync.Ocarina; server.json "ocarina".
#include "Sync.h"

#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"

#include "common/PlayerState.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr float kHearDist = 1500.f;

// After our Link updates (the puppets took this frame's poses).
void Tick(Actor* linkActor) {
    PlayState* play = gPlayState;
    if (play == nullptr || linkActor == nullptr || !Sync_On(SyncPart::Ocarina) || Coop_OcarinaLocalBusy()) {
        Coop_OcarinaRemoteStop();
        return;
    }
    Actor* best = nullptr;
    PlayerState bestState;
    float bestDist = kHearDist;
    for (const auto& [id, remote] : Session_Players()) {
        Actor* puppet = PuppetManager_Actor(id);
        PlayerState st;
        if (puppet == nullptr || puppet->update == nullptr || !PuppetManager_GetCurrent(id, st) ||
            st.ocarinaInstrument == 0) {
            continue;
        }
        float d = Actor_WorldDistXYZToActor(linkActor, puppet);
        if (d < bestDist) {
            best = puppet;
            bestState = st;
            bestDist = d;
        }
    }
    if (best == nullptr) {
        Coop_OcarinaRemoteStop();
        return;
    }
    Coop_OcarinaRemote(bestState.ocarinaInstrument, bestState.ocarinaPitch, bestState.ocarinaBend / 4096.f,
                       bestState.ocarinaVibrato, &best->projectedPos);
}

void RegisterOcarinaEcho() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, Tick);
    COND_HOOK(OnPlayDestroy, true, []() { Coop_OcarinaRemoteStop(); });
}

} // namespace

static RegisterShipInitFunc sOcarinaEchoInit(RegisterOcarinaEcho);

} // namespace coop::client

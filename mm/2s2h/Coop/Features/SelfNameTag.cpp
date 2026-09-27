// Optional nick above our own Link (menu option gCoop.ShowOwnNameTag).
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/NameTag/NameTag.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "variables.h"
}

namespace {

constexpr const char* kTag = "coop-self";
Actor* sTaggedActor = nullptr;

void Untag() {
    if (sTaggedActor != nullptr) {
        NameTag_RemoveAllByTag(kTag);
        sTaggedActor = nullptr;
    }
}

void SelfNameTagTick() {
    bool wanted = coop::client::Session_IsConnected() && CVarGetInteger("gCoop.ShowOwnNameTag", 0) &&
                  PoseCapture_InGameplay();
    Actor* player = wanted ? &GET_PLAYER(gPlayState)->actor : nullptr;
    if (player == sTaggedActor) {
        return;
    }
    Untag();
    if (player != nullptr) {
        NameTagOptions options = {};
        options.tag = kTag;
        options.yOffset = 52;
        NameTag_RegisterForActorWithOptions(player, coop::client::Session_LocalNick().c_str(), options);
        sTaggedActor = player;
    }
}

void RegisterSelfNameTag() {
    COND_HOOK(OnGameStateMainFinish, true, SelfNameTagTick);
    // The player actor is freed with the scene; its tag goes with it (NameTag listens to actor destroy).
    COND_HOOK(OnPlayDestroy, true, []() { sTaggedActor = nullptr; });
}

} // namespace

static RegisterShipInitFunc sSelfNameTagInit(RegisterSelfNameTag);

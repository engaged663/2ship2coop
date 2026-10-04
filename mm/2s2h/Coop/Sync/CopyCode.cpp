// [COOP] Sincronización total §2.4 / §4.1: the code of a copy (an actor another game simulates) only runs here for its
// Init (the copies we create), its Draw and its Destroy. Meanwhile it is silent (its owner sends its sounds), its
// Init makes no particles (its owner sends them) and it queues no cutscene here (its owner plays them: a queued one
// nobody starts would hold back every other cutscene of our game). A player's object (PlayerObjects.cpp) sees that
// player as GET_PLAYER (their puppet: the hookshot draws its chain to their hand) and our globals it would touch are
// given back (the powder keg's flag and timer, the magic, the Elegy's statues and their respawn points). Copies and
// puppets never register attacks here (Coop_BlockAT).
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <cstring>
#include <unordered_map>
#include <vector>

extern "C" {
#include "variables.h"
}

namespace coop::client {

namespace {

struct Guard {
    bool copy = false;
    bool init = false;
    bool keptGlobals = false; // one entry of sKept is its own
    Player* override = nullptr;
};

// Our globals a player's object could touch, given back after its code ran.
struct Kept {
    u8 actorCtxFlags = 0;   // the powder keg's flag (EnBom)
    s16 kegTimer = 0;
    s8 magic = 0;           // magic arrows, the spin attack
    s16 magicState = 0;
    decltype(ActorContext::elegyShells) elegyShells = {}; // EnTorch2_Destroy clears its player's statue...
    RespawnData respawn[RESPAWN_MODE_MAX] = {};           // ...and sets its respawn point
};

std::vector<Guard> sGuards;
std::vector<Kept> sKept;
int sCopyInits = 0;
int sCopyDepth = 0; // copies' own code running now (nested: a copy's Init creating another copy)
// GET_PLAYER of a player's object whose player has no puppet here (it lags, changes form, just left): a Player with
// nothing in it, so that code never reads or writes our own Link (EnBoom_Destroy writes its player's fins and flags)
Player sStandIn;
std::unordered_map<const Actor*, uint8_t> sDying; // copies already untracked whose Destroy has not run (-> owner)

void Keep() {
    Kept k;
    k.actorCtxFlags = gPlayState->actorCtx.flags;
    k.kegTimer = gSaveContext.powderKegTimer;
    k.magic = gSaveContext.save.saveInfo.playerData.magic;
    k.magicState = gSaveContext.magicState;
    std::memcpy(k.elegyShells, gPlayState->actorCtx.elegyShells, sizeof(k.elegyShells));
    std::memcpy(k.respawn, gSaveContext.respawn, sizeof(k.respawn));
    sKept.push_back(k);
}

void GiveBack() {
    if (sKept.empty()) {
        return;
    }
    const Kept& k = sKept.back();
    if (gPlayState != nullptr) {
        gPlayState->actorCtx.flags = k.actorCtxFlags;
        gSaveContext.powderKegTimer = k.kegTimer;
        gSaveContext.save.saveInfo.playerData.magic = k.magic;
        gSaveContext.magicState = k.magicState;
        std::memcpy(gPlayState->actorCtx.elegyShells, k.elegyShells, sizeof(k.elegyShells));
        std::memcpy(gSaveContext.respawn, k.respawn, sizeof(k.respawn));
    }
    sKept.pop_back();
}

void RegisterCopyCode() {
    COND_VB_SHOULD(VB_QUEUE_CUTSCENE, true, {
        if (sCopyDepth > 0) {
            *should = false; // a copy's cutscene plays in its owner's game (Cinema.cpp shows it to who watches)
        }
    });
    COND_HOOK(OnPlayDestroy, true, []() {
        sDying.clear();
        sKept.clear();
    });
}

} // namespace

void CopyCode_NoteDyingCopy(const Actor* actor, uint8_t owner) {
    sDying[actor] = owner;
}

void CopyCode_Begin(Actor* actor, bool init) {
    Guard g;
    uint8_t owner = 0;
    if (actor != nullptr && WorldSession_InWorld()) {
        if (TrackedActor* t = ActorRegistry_Get(actor)) {
            g.copy = Leases_IsRemote(*t) && (!init || t->runtime); // a list actor's Init runs in every game
            owner = t->owner;
        } else if (auto dying = sDying.find(actor); dying != sDying.end() && !init) {
            g.copy = true;
            owner = dying->second;
        }
    }
    if (g.copy) {
        g.init = init;
        g.override = gCoopPlayerOverride;
        SoundEcho_SilenceBegin();
        sCopyInits += init ? 1 : 0;
        sCopyDepth++;
        if (owner != 0 && gPlayState != nullptr) {
            Actor* puppet = PuppetManager_Actor(owner);
            if (puppet == nullptr) {
                std::memset(&sStandIn, 0, sizeof(sStandIn));
                puppet = &sStandIn.actor;
            }
            gCoopPlayerOverride = (Player*)puppet;
            g.keptGlobals = true;
            Keep();
        }
    }
    sGuards.push_back(g);
}

void CopyCode_End() {
    if (sGuards.empty()) {
        return;
    }
    Guard g = sGuards.back();
    sGuards.pop_back();
    if (!g.copy) {
        return;
    }
    SoundEcho_SilenceEnd();
    sCopyInits -= g.init ? 1 : 0;
    sCopyDepth -= sCopyDepth > 0 ? 1 : 0;
    gCoopPlayerOverride = g.override;
    if (g.keptGlobals) {
        GiveBack();
    }
}

bool CopyCode_InCopyInit() {
    return sCopyInits > 0;
}

static RegisterShipInitFunc sCopyCodeInit(RegisterCopyCode);

} // namespace coop::client

using namespace coop::client;

extern "C" void Coop_ActorDrawBegin(Actor* actor) {
    CopyCode_Begin(actor, false);
}

extern "C" void Coop_ActorDrawEnd(Actor* actor) {
    CopyCode_End();
}

extern "C" void Coop_ActorDestroyBegin(Actor* actor) {
    CopyCode_Begin(actor, false);
}

extern "C" void Coop_ActorDestroyEnd(Actor* actor) {
    CopyCode_End();
    sDying.erase(actor);
}

extern "C" s32 Coop_BlockAT(Collider* collider) {
    if (collider == nullptr || collider->actor == nullptr || !WorldSession_InWorld()) {
        return 0;
    }
    Actor* a = collider->actor;
    if (a->id == ACTOR_EN_COOP_PUPPET) {
        return 1;
    }
    TrackedActor* t = ActorRegistry_Get(a);
    return (t != nullptr && Leases_IsRemote(*t)) ? 1 : 0;
}

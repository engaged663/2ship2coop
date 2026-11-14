// [COOP] The magic meter never stays waiting with nothing that will end it (common/MagicRules.h): Magic_Consume only
// works from the idle state, and a waiting state whose arrow, spin attack or form went away (our Link grabbed,
// followed, teleported, its world rebuilt...) left Link without magic until the game restarted. After 3 seconds like
// that the meter goes back to idle, as Magic_Reset does when an arrow is put away. gCoop.MagicGuard (on by default).
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/MagicRules.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "functions.h"
#include "variables.h"
}

static_assert(MAGIC_STATE_IDLE == coop::kMagicIdle && MAGIC_STATE_CONSUME_SETUP == coop::kMagicConsumeSetup &&
                  MAGIC_STATE_CONSUME == coop::kMagicConsume && MAGIC_STATE_METER_FLASH_1 == coop::kMagicMeterFlash1 &&
                  MAGIC_STATE_METER_FLASH_2 == coop::kMagicMeterFlash2 && MAGIC_STATE_RESET == coop::kMagicReset &&
                  MAGIC_STATE_METER_FLASH_3 == coop::kMagicMeterFlash3 &&
                  MAGIC_STATE_CONSUME_LENS == coop::kMagicConsumeLens &&
                  MAGIC_STATE_STEP_CAPACITY == coop::kMagicStepCapacity && MAGIC_STATE_FILL == coop::kMagicFill &&
                  MAGIC_STATE_CONSUME_GORON_ZORA_SETUP == coop::kMagicGoronZoraSetup &&
                  MAGIC_STATE_CONSUME_GORON_ZORA == coop::kMagicGoronZora &&
                  MAGIC_STATE_CONSUME_GIANTS_MASK == coop::kMagicGiantsMask,
              "common/MagicRules.h: the engine's MagicState values");

namespace coop::client {

namespace {

MagicGuard sGuard;

// What resets the meter when it is done (its Destroy or its update calls Magic_Reset). ADD A LINE.
bool EndsMagic(int16_t id) {
    switch (id) {
        case ACTOR_EN_ARROW: // magic arrows and the Deku bubble
        case ACTOR_ARROW_FIRE:
        case ACTOR_ARROW_ICE:
        case ACTOR_ARROW_LIGHT:
        case ACTOR_EN_M_THUNDER: // the spin attack
        case ACTOR_OCEFF_SPOT:   // the songs' effects
        case ACTOR_OCEFF_STORM:
        case ACTOR_OCEFF_WIPE:
        case ACTOR_OCEFF_WIPE2:
        case ACTOR_OCEFF_WIPE3:
        case ACTOR_OCEFF_WIPE4:
        case ACTOR_OCEFF_WIPE5:
        case ACTOR_OCEFF_WIPE6:
        case ACTOR_OCEFF_WIPE7:
            return true;
        default:
            return false;
    }
}

// One of ours alive (a copy of another player's never resets our meter: CopyCode.cpp gives it back).
bool OurItemAlive(PlayState* play) {
    for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
        for (Actor* a = play->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
            if (a->update == nullptr || !EndsMagic(a->id)) {
                continue;
            }
            const TrackedActor* t = ActorRegistry_Get(a);
            if (t == nullptr || !Leases_IsRemote(*t)) {
                return true;
            }
        }
    }
    return false;
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (play == nullptr || !WorldSession_InWorld() || HostMode_Enabled() ||
        CVarGetInteger("gCoop.MagicGuard", 1) == 0) {
        sGuard = MagicGuard{};
        return;
    }
    Player* link = (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    s16 state = gSaveContext.magicState;
    if (link == nullptr || state == MAGIC_STATE_IDLE) {
        sGuard.Frame(false);
        return;
    }
    MagicHolders holders;
    holders.goronOrZora = link->transformation == PLAYER_FORM_GORON || link->transformation == PLAYER_FORM_ZORA;
    holders.giantsMask = link->currentMask == PLAYER_MASK_GIANT;
    bool orphaned = Magic_Orphaned(state, holders);
    if (orphaned) {
        holders.item = OurItemAlive(play); // (only looked for when it matters)
        orphaned = Magic_Orphaned(state, holders);
    }
    if (sGuard.Frame(orphaned)) {
        SPDLOG_WARN("[Coop] The magic meter waited in state {} with nothing to end it: back to idle", (int)state);
        Magic_Reset(play);
    }
}

void RegisterMagicGuard() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    COND_HOOK(OnPlayDestroy, true, []() { sGuard = MagicGuard{}; });
}

} // namespace

static RegisterShipInitFunc sMagicGuardInit(RegisterMagicGuard);

} // namespace coop::client

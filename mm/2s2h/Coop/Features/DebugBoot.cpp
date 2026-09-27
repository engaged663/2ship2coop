// Test aid (gCoop.Debug.BootToClockTown): boots straight into South Clock Town with the debug file
// (every item and mask, never written to disk), so the co-op features can be tried in seconds.
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/gamestates/ovl_select/z_select.h"
}

#define CVAR_NAME "gCoop.Debug.BootToClockTown"

static void RegisterDebugBoot() {
    COND_HOOK(OnConsoleLogoUpdate, CVarGetInteger(CVAR_NAME, 0), []() {
        if (gPlayState != nullptr) {
            return;
        }
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = 0xFF; // debug file slot: MapSelect_LoadGame creates the debug save
        MapSelect_LoadGame((MapSelectState*)gGameState, ENTRANCE(SOUTH_CLOCK_TOWN, 0), 0);
    });
}

static RegisterShipInitFunc sDebugBootInit(RegisterDebugBoot, { CVAR_NAME });

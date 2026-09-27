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
// Optional: another entrance id to boot into (e.g. 54784 = North Clock Town). Default: South Clock Town.
#define CVAR_ENTRANCE "gCoop.Debug.BootEntrance"

static void RegisterDebugBoot() {
    COND_HOOK(OnConsoleLogoUpdate, CVarGetInteger(CVAR_NAME, 0), []() {
        if (gPlayState != nullptr) {
            return;
        }
        gSaveContext.gameMode = GAMEMODE_NORMAL;
        gSaveContext.fileNum = 0xFF; // debug file slot: MapSelect_LoadGame creates the debug save
        u16 entrance = (u16)CVarGetInteger(CVAR_ENTRANCE, ENTRANCE(SOUTH_CLOCK_TOWN, 0));
        MapSelect_LoadGame((MapSelectState*)gGameState, entrance, 0);
    });
}

static RegisterShipInitFunc sDebugBootInit(RegisterDebugBoot, { CVAR_NAME });

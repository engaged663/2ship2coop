// Core of the co-op mod inside the game: pumps the network once per frame. Every feature file
// (chat, puppets, location...) registers its own hooks with RegisterShipInitFunc.
// Map of the module: coop/README.md.
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

static void RegisterCoop() {
    // Network messages are handled at the start of every game frame, before actors update.
    COND_HOOK(OnGameStateMainStart, true, []() { coop::client::ProcessNetwork(); });

    static bool sAutoConnected = false; // presets run this function again
    if (!sAutoConnected && CVarGetInteger("gCoop.AutoConnect", 0)) {
        sAutoConnected = true;
        coop::client::Session_ConnectFromSettings();
    }
}

static RegisterShipInitFunc sCoopInit(RegisterCoop);

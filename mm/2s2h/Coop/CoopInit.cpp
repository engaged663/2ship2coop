// Core of the co-op mod inside the game: the order of the per-frame work. Every feature file (chat, puppets,
// location...) registers its own hooks with RegisterShipInitFunc. Map of the module: coop/README.md.
#include "2s2h/Coop/Actors/ActorSync.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/O2r/O2r.h"
#include "2s2h/Coop/World/ClockSync.h"
#include "2s2h/Coop/World/WorldSession.h"
#include "2s2h/Coop/World/WorldSync.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

static void RegisterCoop() {
    // Every frame starts with the network (before actors update), then the shared world acts on its messages...
    COND_HOOK(OnGameStateMainStart, true, []() {
        coop::client::ProcessNetwork();
        coop::client::O2r_FrameStart(); // the server's .o2r mods: before the world, which waits for them
        coop::client::WorldSession_FrameStart();
        coop::client::ClockSync_FrameStart();
    });
    // ...and ends by sending what changed in the world and the state of the enemies we simulate.
    COND_HOOK(OnGameStateMainFinish, true, []() {
        coop::client::WorldSync_FrameEnd();
        coop::client::WorldSession_FrameEnd();
        coop::client::ActorSync_FrameEnd();
    });

    static bool sAutoConnected = false; // presets run this function again
    if (!sAutoConnected && CVarGetInteger("gCoop.AutoConnect", 0)) {
        sAutoConnected = true;
        coop::client::Session_ConnectFromSettings();
    }
}

static RegisterShipInitFunc sCoopInit(RegisterCoop);

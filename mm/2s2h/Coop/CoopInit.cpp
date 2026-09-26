// Entry point of the co-op mod inside the game: registers the per-frame hooks once at startup.
// Map of the module: coop/README.md.
#include "2s2h/Coop/Chat/ChatWindow.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

static void RegisterCoop() {
    // Network messages are handled at the start of every game frame, before actors update.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainStart>(
        []() { coop::client::ProcessNetwork(); });

    coop::client::ChatWindow_Register();

    if (CVarGetInteger("gCoop.AutoConnect", 0)) {
        coop::client::Session_ConnectFromSettings();
    }
}

static RegisterShipInitFunc sCoopInit(RegisterCoop);

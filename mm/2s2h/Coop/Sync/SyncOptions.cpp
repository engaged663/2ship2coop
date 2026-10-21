// [COOP] See Sync.h: the switches of the total sync. Each part works when its CVar is on (1 by default) and the
// server allows it ("sync" of "welcome"; a server that says nothing allows everything).
#include "Sync.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/I18n/CoopText.h"
#include "2s2h/Coop/World/WorldSession.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <array>

namespace coop::client {

namespace {

constexpr size_t kParts = (size_t)SyncPart::Count;
constexpr std::array<const char*, kParts> kCVars = { "gCoop.Sync.Sounds",        "gCoop.Sync.Ambient",
                                                     "gCoop.Sync.PlayerObjects", "gCoop.Sync.SceneFlags",
                                                     "gCoop.Sync.SceneObjects",  "gCoop.Sync.Ocarina",
                                                     "gCoop.Sync.Drops" };
constexpr std::array<const char*, kParts> kKeys = { "sounds",     "ambient",      "playerObjects",
                                                    "sceneFlags", "sceneObjects", "ocarina",
                                                    "drops" };
constexpr std::array<Txt, kParts> kNames = { Txt::SyncSounds,       Txt::SyncAmbient,      Txt::SyncPlayerObjects,
                                             Txt::SyncSceneFlags,   Txt::SyncSceneObjects, Txt::SyncOcarina,
                                             Txt::SyncDrops };
std::array<bool, kParts> sServer = { true, true, true, true, true, true, true };

void OnWelcome(const json& ev) {
    auto sync = ev.find("sync");
    for (size_t i = 0; i < kParts; i++) {
        bool allowed = true;
        if (sync != ev.end() && sync->is_object()) {
            auto v = sync->find(kKeys[i]);
            allowed = v == sync->end() || !v->is_boolean() || v->get<bool>();
        }
        sServer[i] = allowed;
    }
}

void OnLost(const std::string&) {
    sServer.fill(true);
}

} // namespace

bool Sync_ServerAllows(SyncPart part) {
    return sServer[(size_t)part];
}

bool Sync_On(SyncPart part) {
    return WorldSession_Active() && sServer[(size_t)part] && CVarGetInteger(kCVars[(size_t)part], 1) != 0;
}

const char* Sync_CVar(SyncPart part) {
    return kCVars[(size_t)part];
}

const char* Sync_Name(SyncPart part) {
    return Tc(kNames[(size_t)part]);
}

} // namespace coop::client

COOP_ON_EVENT(syncWelcome, coop::ev::kWelcome, coop::client::OnWelcome);
COOP_ON_LOST(syncLost, coop::client::OnLost);

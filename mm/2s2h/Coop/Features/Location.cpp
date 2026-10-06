// Tells the server which scene/room we are in ("loc"): used for /list, the player list and /tp, and whether the
// original game stops time there (the shared clock waits while someone is in such a scene, e.g. the Moon).
#include "2s2h/Coop/Actors/Leases.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Cinema.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/Room/Room.h"

#include "common/PlayerState.h"
#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/ShipUtils.h"

#include <algorithm>

extern "C" {
#include "variables.h"
}

namespace {

int16_t sLastScene = -2;
int8_t sLastRoom = -2;
int8_t sLastLayer = -2; // gSaveContext.sceneLayer: the server shares actors and flags per scene + layer
bool sLastTimeStopped = false;
bool sLastBusy = false;

// In a cutscene: the enemies we simulate would freeze for everyone, so the server hands our rooms to someone else
// meanwhile (sub-project C). The pause menu, texts and the ocarina stop nothing in the server's world. A boss's room
// is the exception (its cutscenes run in the game that runs it, and it must never change hands in the middle), and so
// is a cutscene of ours the whole scene is watching (a miniboss's), and the cutscene of any shared actor we simulate
// (only its own update ends it: Bomber Jim's was left running in both games when he changed hands in the middle).
// A game frozen while its activity room waits for everyone (Room/RoomHold.cpp) is busy too: what it simulates would
// stand still for the others.
bool Busy() {
    if (coop::client::RoomHold_Active()) {
        return true;
    }
    return Player_InCsMode(gPlayState) && !coop::client::BossArena_Is(gPlayState->sceneId) &&
           !coop::client::Cinema_DirectingScene() && !coop::client::Leases_HoldsCutscene();
}

void Reset() {
    sLastScene = -2;
    sLastRoom = -2;
    sLastLayer = -2;
    sLastTimeStopped = false;
    sLastBusy = false;
}

void LocationTick() {
    if (!coop::client::Session_IsConnected() || !PoseCapture_InGameplay()) {
        return;
    }
    int16_t scene = gPlayState->sceneId;
    int8_t room = gPlayState->roomCtx.curRoom.num;
    uint8_t layer = (uint8_t)std::clamp<int>(gSaveContext.sceneLayer, 0, coop::pose_limits::kMaxLayer);
    // The original stops time in some scenes (the Moon); from Oath to Order on the giants hold the moon
    bool timeStopped = gPlayState->envCtx.sceneTimeSpeed == 0 || coop::client::Ending_StopsTime();
    bool busy = Busy();
    if (scene == sLastScene && room == sLastRoom && layer == sLastLayer && timeStopped == sLastTimeStopped &&
        busy == sLastBusy) {
        return;
    }
    sLastBusy = busy;
    sLastScene = scene;
    sLastRoom = room;
    sLastLayer = (int8_t)layer;
    sLastTimeStopped = timeStopped;
    coop::json ev = coop::MakeEvent(coop::ev::kLoc);
    ev["scene"] = scene;
    ev["room"] = room;
    ev["layer"] = layer;
    ev["entrance"] = gSaveContext.save.entrance;
    const char* name = Ship_GetSceneName(scene);
    ev["sceneName"] = name != nullptr ? name : "";
    ev["timeStopped"] = timeStopped;
    ev["busy"] = busy;
    coop::client::NetClient::Get().SendEvent(ev);
}

void OnWelcome(const coop::json& ev) {
    Reset();
}

void OnLost(const std::string& reason) {
    Reset();
}

void RegisterLocation() {
    COND_HOOK(OnGameStateMainFinish, true, LocationTick);
}

} // namespace

COOP_ON_EVENT(locationWelcome, coop::ev::kWelcome, OnWelcome);
COOP_ON_LOST(locationLost, OnLost);
static RegisterShipInitFunc sLocationInit(RegisterLocation);

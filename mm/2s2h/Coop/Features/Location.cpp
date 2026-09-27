// Tells the server which scene/room we are in ("loc"): used for /list, the player list and /tp, and whether the
// original game stops time there (the shared clock waits while someone is in such a scene, e.g. the Moon).
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/ShipUtils.h"

extern "C" {
#include "variables.h"
}

namespace {

int16_t sLastScene = -2;
int8_t sLastRoom = -2;
bool sLastTimeStopped = false;
bool sLastBusy = false;

// Paused, reading a text or in a cutscene: the enemies we simulate would freeze for everyone, so the server hands
// our rooms to someone else meanwhile (sub-project C).
bool Busy() {
    return IS_PAUSED(&gPlayState->pauseCtx) || gPlayState->msgCtx.msgMode != MSGMODE_NONE ||
           Player_InCsMode(gPlayState);
}

void Reset() {
    sLastScene = -2;
    sLastRoom = -2;
    sLastTimeStopped = false;
    sLastBusy = false;
}

void LocationTick() {
    if (!coop::client::Session_IsConnected() || !PoseCapture_InGameplay()) {
        return;
    }
    int16_t scene = gPlayState->sceneId;
    int8_t room = gPlayState->roomCtx.curRoom.num;
    bool timeStopped = gPlayState->envCtx.sceneTimeSpeed == 0;
    bool busy = Busy();
    if (scene == sLastScene && room == sLastRoom && timeStopped == sLastTimeStopped && busy == sLastBusy) {
        return;
    }
    sLastBusy = busy;
    sLastScene = scene;
    sLastRoom = room;
    sLastTimeStopped = timeStopped;
    coop::json ev = coop::MakeEvent(coop::ev::kLoc);
    ev["scene"] = scene;
    ev["room"] = room;
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

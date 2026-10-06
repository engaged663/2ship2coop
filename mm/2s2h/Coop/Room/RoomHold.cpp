// [COOP] The director's game waits for its room (spec §5.2). From the moment it opens one (its activity started here)
// until the room runs, closes or the server says there is none, its world is frozen: z_play.c asks Coop_PlayHeld and
// skips the world's update (actors, texts, the transition, the HUD), and COOP_WORLD_PAUSED pauses the minigame timers
// (they run on the system clock) as the original pause does. It is only held in the scene where it began (if that
// scene goes away, the round cannot start there: it leaves the room), never in the middle of a transition, in the
// ending or in the server's own games. START says "ready" (or takes it back) meanwhile: the pause menu cannot open.
#include "Room.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Host/HostMode.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <string>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kAnswerMs = 3000; // a room asked for and not answered: wait this long at most

std::string sKey;          // the activity whose start waits ("" = none)
int16_t sScene = -1;       // ...in this scene
bool sPending = false;     // we asked for its room and the server has not answered yet
uint32_t sPendingSerial = 0;
int64_t sPendingSinceMs = 0;
bool sWasHeld = false;

bool Waiting(RoomPhase phase) {
    return phase == RoomPhase::Lobby || phase == RoomPhase::Starting;
}

// Our room waits for its confirmations, or the server has not answered our "open" yet.
bool RoomWaitsForUs() {
    const RoomInfo& r = Room_Get();
    if (sPending) {
        bool answered = Room_Serial() != sPendingSerial && (r.id == 0 || r.key == sKey);
        if (answered || Group_NowMs() - sPendingSinceMs > kAnswerMs) {
            sPending = false; // a room, none ("off") or nobody answers
        } else {
            return true;
        }
    }
    return r.id != 0 && r.key == sKey && r.director == Session_LocalId() && Waiting(r.phase);
}

bool HeldNow(PlayState* play) {
    if (play == nullptr || sKey.empty() || !Room_HoldEnabled() || !WorldSession_Active() || HostMode_Enabled() ||
        EndingMode_Active() || play->sceneId != sScene || play->transitionMode != TRANS_MODE_OFF) {
        return false;
    }
    return RoomWaitsForUs();
}

void FrameEnd() {
    PlayState* play = gPlayState;
    bool held = HeldNow(play);
    if (held && !sWasHeld) {
        SPDLOG_INFO("[Coop] Waiting for room {} to be ready", sKey);
        Chat_Add(ChatKind::Info, "Tu partida espera a que todos los de la sala estén listos (START: listo).");
        RoomWindow_Show();
    } else if (!held && sWasHeld) {
        SPDLOG_INFO("[Coop] Room {} no longer holds this game", sKey);
    }
    if (held && CHECK_BTN_ALL(CONTROLLER1(&play->state)->press.button, BTN_START)) {
        const RoomMemberInfo* me = Room_Me();
        Room_SetReady(me == nullptr || !me->ready);
    }
    sWasHeld = held;
    if (sKey.empty() || held || sPending) {
        return;
    }
    const RoomInfo& r = Room_Get();
    bool ours = r.id != 0 && r.key == sKey && r.director == Session_LocalId() && Waiting(r.phase);
    if (ours && play != nullptr && play->sceneId != sScene && WorldSession_Active()) {
        // The scene where the round waited is gone (a reset of the cycle, the end of the world): it cannot start
        Chat_Add(ChatKind::Warn, "La actividad no puede empezar ya: sales de su sala.");
        Room_Leave();
        ours = false;
    }
    if (!ours) {
        sKey.clear(); // nothing waits for us any more
    }
}

void Reset() {
    sKey.clear();
    sScene = -1;
    sPending = false;
    sWasHeld = false;
}

void RegisterRoomHold() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

void RoomHold_Begin(const std::string& key) {
    if (!Room_HoldEnabled() || gPlayState == nullptr) {
        return;
    }
    sKey = key;
    sScene = gPlayState->sceneId;
    sPending = true;
    sPendingSerial = Room_Serial();
    sPendingSinceMs = Group_NowMs();
}

bool RoomHold_Active() {
    return HeldNow(gPlayState);
}

} // namespace coop::client

extern "C" s32 Coop_PlayHeld(PlayState* play) {
    return coop::client::HeldNow(play) ? 1 : 0;
}

COOP_ON_EVENT(roomHoldWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(roomHoldLost, [](const std::string&) { coop::client::Reset(); });
static RegisterShipInitFunc sRoomHoldInit(coop::client::RegisterRoomHold);

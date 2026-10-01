// [COOP] The Clock Tower rooftop's countdown (Skull Kid starts 5 minutes when you arrive on the final night) is the
// server's in the shared world: the first game that starts it tells the server, and every game shows what is left
// of that one ("rooftop" events), even the ones that are not on the rooftop. Oath to Order stops it (Ending.cpp);
// when it runs out the server makes the moon fall for everyone. The game's own countdown is overwritten every frame
// (the pause menu does not stop it: the world keeps running), and never makes the moon fall by itself.
#include "Ending.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <chrono>

extern "C" {
#include "functions.h"
#include "regs.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr s16 kTimerX = 115; // where Skull Kid puts it (R_MOON_CRASH_TIMER_X/Y)
constexpr s16 kTimerY = 200;

int64_t sDeadlineMs = 0; // 0: no countdown
bool sStartSent = false; // this game told the server about its countdown
u16 sPrevState = TIMER_STATE_OFF;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void TimerOff() {
    gSaveContext.timerStates[TIMER_ID_MOON_CRASH] = TIMER_STATE_OFF;
    sPrevState = TIMER_STATE_OFF;
}

// The game's countdown shows what is left of the server's.
void Show() {
    int64_t left = sDeadlineMs - NowMs();
    if (left <= 0) {
        return; // over: the game's own timer ends at 0 (and the server makes the moon fall)
    }
    OSTime centiseconds = (OSTime)(left / 10);
    if (R_MOON_CRASH_TIMER_X == 0 && R_MOON_CRASH_TIMER_Y == 0) {
        R_MOON_CRASH_TIMER_X = kTimerX;
        R_MOON_CRASH_TIMER_Y = kTimerY;
    }
    gSaveContext.timerDirections[TIMER_ID_MOON_CRASH] = TIMER_COUNT_DOWN;
    gSaveContext.timerTimeLimits[TIMER_ID_MOON_CRASH] = centiseconds;
    gSaveContext.timerCurTimes[TIMER_ID_MOON_CRASH] = centiseconds;
    gSaveContext.timerStartOsTimes[TIMER_ID_MOON_CRASH] = osGetTime();
    gSaveContext.timerPausedOsTimes[TIMER_ID_MOON_CRASH] = 0;
    if (gSaveContext.timerStates[TIMER_ID_MOON_CRASH] != TIMER_STATE_COUNTING) {
        gSaveContext.timerStates[TIMER_ID_MOON_CRASH] = TIMER_STATE_COUNTING;
        gSaveContext.timerX[TIMER_ID_MOON_CRASH] = R_MOON_CRASH_TIMER_X;
        gSaveContext.timerY[TIMER_ID_MOON_CRASH] = R_MOON_CRASH_TIMER_Y;
    }
    sPrevState = TIMER_STATE_COUNTING;
}

void FrameEnd() {
    if (!WorldSession_Active() || EndingMode_Active() || gPlayState == nullptr) {
        return;
    }
    if (sDeadlineMs != 0) {
        Show();
        return;
    }
    u16 state = gSaveContext.timerStates[TIMER_ID_MOON_CRASH];
    if (state != TIMER_STATE_OFF && sPrevState == TIMER_STATE_OFF && !sStartSent &&
        gPlayState->sceneId == SCENE_OKUJOU) {
        // Skull Kid started it here: the server keeps the first one and tells everyone what is left
        int64_t ms = (int64_t)gSaveContext.timerTimeLimits[TIMER_ID_MOON_CRASH] * 10;
        json ev = MakeEvent(ev::kRooftop);
        ev["ms"] = std::clamp<int64_t>(ms, 1000, kRooftopMaxMs);
        NetClient::Get().SendEvent(ev);
        sStartSent = true;
    }
    sPrevState = state;
}

void OnRooftop(const json& ev) {
    if (WorldSession_State() == WorldState::Outside) {
        return;
    }
    int64_t ms = GetInt(ev, "ms", -1);
    if (ms < 0) {
        sDeadlineMs = 0;
        sStartSent = false;
        TimerOff();
        return;
    }
    sDeadlineMs = NowMs() + std::min<int64_t>(ms, kRooftopMaxMs);
}

void Reset() {
    sDeadlineMs = 0;
    sStartSent = false;
    sPrevState = TIMER_STATE_OFF;
}

void RegisterRooftopTimer() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

void RooftopTimer_Stop() {
    Reset();
    TimerOff();
}

COOP_ON_EVENT(rooftopEvent, coop::ev::kRooftop, OnRooftop);
COOP_ON_EVENT(rooftopWorld, coop::ev::kWorldFull, [](const json&) { RooftopTimer_Stop(); }); // a new cycle
COOP_ON_LOST(rooftopLost, [](const std::string&) { Reset(); });
static RegisterShipInitFunc sRooftopTimerInit(RegisterRooftopTimer);

} // namespace coop::client

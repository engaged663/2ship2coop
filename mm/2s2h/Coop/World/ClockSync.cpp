#include "ClockSync.h"

#include "FieldTable.h"
#include "WorldSession.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Features/Ending.h"
#include "2s2h/Coop/Features/Warp.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"

#include "common/Clock.h"
#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

extern "C" {
#include "functions.h"
#include "variables.h"
}

// DeveloperTools/SaveEditor.cpp: sets the time and resyncs what depends on it (En_Test4, day/night actors, music).
void UpdateGameTime(u16 gameTime);

namespace coop::client {

namespace {

constexpr int kSpeedCooldownMs = 2000; // at most one Inverted Song request every 2 s
constexpr int kDayGraceMs = 5000;      // the day counter may disagree this long (the new-day screen fixes it)
constexpr int kReloadWaitMs = 10000;   // after a jump the scene reloads as soon as Link can be moved (a dialog...)

bool sHave = false;
uint32_t sAbs = 0;
bool sInverted = false;
bool sStopped = true;
double sUps = clock::kUnitsPerSecond; // how fast the server's clock runs: units a second (its "ups": the speed of the
                                      // world, server.json timeSpeed, and the Inverted Song of Time together)
int64_t sAtMs = 0;
int64_t sSpeedAllowedAtMs = 0;
int64_t sDayWrongSinceMs = 0;
int64_t sReloadUntilMs = 0; // != 0: reload the scene after a jump, as soon as possible (until then)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

uint32_t ServerAbs() {
    if (sStopped) {
        return sAbs;
    }
    int64_t elapsed = std::clamp<int64_t>(NowMs() - sAtMs, 0, clock::kMaxExtrapolationMs);
    return (uint32_t)std::min<int64_t>(sAbs + (int64_t)((double)elapsed * sUps / 1000.0), clock::kMoonAbs);
}

void Store(const json& serverClock) {
    sAbs = (uint32_t)std::clamp<int64_t>(GetInt(serverClock, "abs", 0), 0, clock::kMoonAbs);
    sInverted = GetBool(serverClock, "inv");
    sStopped = GetBool(serverClock, "stopped", true);
    double ups = GetNumber(serverClock, "ups", -1.0);
    bool valid = std::isfinite(ups) && ups >= 0.0 && ups <= clock::kUnitsPerSecond * kMaxTimeSpeed;
    sUps = valid ? ups : clock::UnitsPerSecond(sInverted);
    sAtMs = NowMs();
    sHave = true;
}

// The game's time placed next to the server's. The day counter only changes on the new-day screen, a moment after
// the time passes 6:00, so it cannot tell which day the time belongs to around the change of day.
uint32_t GameAbs(uint32_t serverAbs) {
    int16_t delta = (int16_t)(uint16_t)(CURRENT_TIME - clock::TimeOfAbs(serverAbs));
    return (uint32_t)std::max<int64_t>((int64_t)serverAbs + delta, 0);
}

// A jump of the server's clock (Song of Double Time, /settime, a day that went wrong).
void ApplyJump(uint32_t abs) {
    int day = clock::DayOfAbs(abs);
    u16 time = clock::TimeOfAbs(abs);
    bool live = WorldSession_Active() && PoseCapture_InGameplay() && fields::PlayLive();
    if (live) {
        for (int d = CURRENT_DAY; d < day; d++) {
            Sram_IncrementDay(); // the rules of every new day (Bombers, two event flags)
        }
    }
    gSaveContext.save.day = day;
    gSaveContext.save.eventDayCount = day;
    gSaveContext.save.isNight = clock::IsNight(time);
    gSaveContext.timerStates[TIMER_ID_MOON_CRASH] = TIMER_STATE_OFF;
    sDayWrongSinceMs = 0;
    if (!live) {
        gSaveContext.save.time = time;
        return;
    }
    UpdateGameTime(time);
    sReloadUntilMs = NowMs() + kReloadWaitMs; // so everything in the scene belongs to the new time
}

void ReloadAfterJump() {
    if (sReloadUntilMs == 0) {
        return;
    }
    WarpTarget here;
    if (Warp_BlockedReason().empty() && Warp_Current(here)) {
        sReloadUntilMs = 0;
        Warp_Go(here);
    } else if (NowMs() > sReloadUntilMs) {
        sReloadUntilMs = 0; // still busy: the next scene change brings the new time
    }
}

// Runs before Message_Update: a "yes" to a song is seen the frame after it is chosen, before the game acts on it.
void InterceptSongs() {
    PlayState* play = gPlayState;
    MessageContext* msgCtx = &play->msgCtx;
    if (msgCtx->msgMode == MSGMODE_NEW_CYCLE_0) {
        // Song of Time: the game would reset the cycle for this player alone. The server holds a vote instead.
        msgCtx->ocarinaMode = OCARINA_MODE_END;
        Message_CloseTextbox(play);
        if (msgCtx->msgMode == MSGMODE_NEW_CYCLE_0) {
            msgCtx->msgMode = MSGMODE_TEXT_CLOSING;
            msgCtx->stateTimer = 2;
        }
        NetClient::Get().SendEvent(MakeEvent(ev::kSotPropose));
        Chat_Add(ChatKind::Info, "Has propuesto volver al Amanecer del Primer Día.");
        return;
    }
    switch (msgCtx->ocarinaMode) {
        case OCARINA_MODE_APPLY_DOUBLE_SOT:
            // The "yes" did not move this game's time (VB_SONG_OF_DOUBLE_TIME_SET_TIME): the server moves everyone's
            msgCtx->ocarinaMode = OCARINA_MODE_END;
            NetClient::Get().SendEvent(MakeEvent(ev::kClockJump));
            break;
        case OCARINA_MODE_APPLY_INV_SOT_FAST:
        case OCARINA_MODE_APPLY_INV_SOT_SLOW:
            msgCtx->ocarinaMode = OCARINA_MODE_END;
            if (NowMs() >= sSpeedAllowedAtMs) {
                sSpeedAllowedAtMs = NowMs() + kSpeedCooldownMs;
                json ev = MakeEvent(ev::kClockSpeed);
                ev["inv"] = !sInverted;
                NetClient::Get().SendEvent(ev);
            }
            break;
        default:
            break;
    }
}

void Follow(uint32_t serverAbs) {
    clock::Follow f = clock::FollowServer(GameAbs(serverAbs), serverAbs, sStopped);
    if (f.write) {
        if (f.backwards) {
            UpdateGameTime(clock::TimeOfAbs(f.writeAbs)); // going back must not look like a new day to En_Test4
        } else {
            gSaveContext.save.time = clock::TimeOfAbs(f.writeAbs); // En_Test4 sees 6:00/18:00 go by as usual
        }
    }
    s32 speed = R_TIME_SPEED;
    gSaveContext.save.timeSpeedOffset = (f.freeze && speed != 0) ? -speed : (sInverted ? -2 : 0);
}

void CheckDay(uint32_t serverAbs) {
    PlayState* play = gPlayState;
    bool settled = serverAbs < clock::kMoonAbs - clock::kClientMoonMargin &&
                   play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF;
    if (!settled || CURRENT_DAY == clock::DayOfAbs(serverAbs)) {
        sDayWrongSinceMs = 0;
        return;
    }
    int64_t now = NowMs();
    if (sDayWrongSinceMs == 0) {
        sDayWrongSinceMs = now;
    } else if (now - sDayWrongSinceMs >= kDayGraceMs) {
        ApplyJump(serverAbs);
    }
}

void OnClock(const json& ev) {
    if (WorldSession_State() == WorldState::Outside) {
        return;
    }
    Store(ev);
    if (GetBool(ev, "jump") && WorldSession_InWorld() && !EndingMode_Active()) {
        ApplyJump(sAbs);
    }
}

void RegisterClockSync() {
    // The moon falls when the server's clock reaches Day 4, 6:00: for everyone, and never for one game alone.
    COND_VB_SHOULD(VB_START_MOON_CRASH, true, {
        if (WorldSession_InWorld()) {
            *should = false;
        }
    });
    // The Song of Double Time asks the server for the jump (InterceptSongs) instead of moving this game's time.
    COND_VB_SHOULD(VB_SONG_OF_DOUBLE_TIME_SET_TIME, true, {
        if (WorldSession_InWorld()) {
            *should = false;
        }
    });
}

} // namespace

void ClockSync_OnFull(const json& serverClock) {
    Store(serverClock);
}

json ClockSync_ServerJson() {
    return json{ { "abs", ServerAbs() }, { "inv", sInverted }, { "stopped", sStopped } };
}

std::string ClockSync_Describe() {
    if (!sHave) {
        return "";
    }
    std::string text = clock::Format(ServerAbs());
    if (sInverted) {
        text += " · tiempo ralentizado";
    }
    if (sStopped) {
        text += " · reloj parado";
    }
    return text;
}

void ClockSync_Reset() {
    sHave = false;
    sDayWrongSinceMs = 0;
    sReloadUntilMs = 0;
}

void ClockSync_FrameStart() {
    if (!sHave || !WorldSession_Active() || !PoseCapture_InGameplay() || EndingMode_Active()) {
        return; // the ending's cutscenes set their own time of day
    }
    uint32_t serverAbs = ServerAbs();
    InterceptSongs();
    Follow(serverAbs);
    CheckDay(serverAbs);
    ReloadAfterJump();
}

COOP_ON_EVENT(clockSyncClock, ev::kClock, OnClock);

} // namespace coop::client

static RegisterShipInitFunc sClockSyncInit(coop::client::RegisterClockSync);

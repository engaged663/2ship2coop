// [COOP] This game runs an activity (spec §3.2, §4). It says what it stands next to ("act here": the invitations and
// /list show it), when one of the table's minigames starts and ends here ("act start/end"), keeps that minigame's NPC
// (Leases.cpp asks Director_PinsActorId; ActorSync.cpp keeps its family on our Link), sends its HUD to the group in
// the scene ("act_hud", shown by Guest.cpp) and takes the mates near it along to the minigame's entrances ("follow").
#include "Activities.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <map>

extern "C" {
#include "functions.h"
#include "sequence.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr int64_t kEndAfterMs = 1500;   // no minigame signal for this long: it is over
constexpr int64_t kRecentMs = 20000;    // its prizes still count this long after it ended
constexpr int kHudEvery = 2;            // "act_hud" 10 times a second (the game runs at 20)
constexpr float kQuestNpcDist = 400.f;  // a quest's NPC this close names our invitations
constexpr float kFollowDist = 3000.f;   // mates this close come along to the minigame's entrance
constexpr int64_t kTalkStickyMs = 5000; // after talking to the minigame's NPC it stays ours (walking to the counter)
constexpr int64_t kResultOnceMs = 5000; // one run, one result (its director may also be a guest of the same race)

std::map<std::string, int64_t> sOwnRuns; // activity key -> when this game last ran it (Por turnos)
int64_t sLastHudCs = 0;                  // the clock of the last HUD sent (the round's time when it ends)
bool sScored = false;                    // it showed points while it ran (else minigameScore is another game's)
std::string sResultKey;                  // the last own result sent, and when
int64_t sResultMs = 0;

const ActivityDef* sRunning = nullptr;
int16_t sRunningScene = -1;
const ActivityDef* sLastRan = nullptr;
int64_t sEndedMs = 0;
int64_t sLastSignalMs = 0;
std::string sHereKey = "-"; // the last "act here/none" sent ("-": none yet)
int sFrames = 0;
bool sPrevStarting = false;
int64_t sTalkedToNpcMs = -kTalkStickyMs;

// The activity whose NPC and props this game keeps: the one it runs, or (just after talking to its NPC) the minigame
// of this scene. Cada uno: each game runs its own race, so its NPC is only kept around the talk that starts it (a
// mate that already finished can talk to it for its prize).
const ActivityDef* Pinned(int16_t scene) {
    if (sRunning != nullptr && sRunning->mode != ActivityMode::EachOwn) {
        return sRunning;
    }
    return Group_NowMs() - sTalkedToNpcMs < kTalkStickyMs ? Activity_MinigameOfScene(scene) : nullptr;
}

Player* Link(PlayState* play) {
    return (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

void Send(const char* state, const ActivityDef* def) {
    json ev = MakeEvent(ev::kAct);
    ev["state"] = state;
    ev["key"] = def != nullptr ? def->key : "";
    ev["name"] = def != nullptr ? def->name : "";
    NetClient::Get().SendEvent(ev);
}

bool TimerOn(int id) {
    return gSaveContext.timerStates[id] != TIMER_STATE_OFF;
}

// The minigame signals of this game (a copy of an NPC never sets them: they are ours).
bool Signals(PlayState* play, const ActivityDef& def) {
    return gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE || TimerOn(TIMER_ID_MINIGAME_1) ||
           TimerOn(TIMER_ID_MINIGAME_2) || TimerOn(TIMER_ID_POSTMAN) ||
           play->interfaceCtx.minigameState != MINIGAME_STATE_NONE ||
           Activity_IsSpecialEntrance(def, gSaveContext.save.entrance);
}

// A quest's NPC we talk to or stand next to (it names our invitations).
const ActivityDef* QuestHere(PlayState* play) {
    Player* link = Link(play);
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_NPC].first; a != nullptr; a = a->next) {
        const ActivityDef* def = Activity_QuestOfNpc(a->id);
        if (def != nullptr && (link->talkActor == a || Actor_WorldDistXYZToActor(&link->actor, a) < kQuestNpcDist)) {
            return def;
        }
    }
    return nullptr;
}

void Start(PlayState* play, const ActivityDef* def) {
    sRunning = def;
    sRunningScene = play->sceneId;
    sOwnRuns[def->key] = Group_NowMs();
    sLastHudCs = 0;
    sScored = false;
    Send("start", def);
    SPDLOG_INFO("[Coop] Activity {} starts in this game", def->key);
    if (Group_AnyMateNear(kFollowDist)) {
        Chat_Add(ChatKind::Info, std::string(def->name) + ": tu grupo juega contigo.");
    }
}

void End() {
    if (sRunning == nullptr) {
        return;
    }
    json ev = MakeEvent(ev::kAct);
    ev["state"] = "end";
    ev["key"] = sRunning->key;
    ev["name"] = sRunning->name;
    // the round's result (Por turnos)
    ev["score"] = sScored ? std::min<int>(gSaveContext.minigameScore, 0xFFFF) : 0;
    ev["cs"] = std::clamp<int64_t>(sLastHudCs, 0, 36000000);
    NetClient::Get().SendEvent(ev);
    SPDLOG_INFO("[Coop] Activity {} ends", sRunning->key);
    sOwnRuns[sRunning->key] = Group_NowMs();
    sLastRan = sRunning;
    sEndedMs = Group_NowMs();
    sRunning = nullptr;
}

// The minigame timer on screen: direction, limit and time gone by, in centiseconds.
void AddTimer(json& ev) {
    for (int id : { (int)TIMER_ID_MINIGAME_1, (int)TIMER_ID_MINIGAME_2, (int)TIMER_ID_POSTMAN }) {
        u8 state = gSaveContext.timerStates[id];
        if (state == TIMER_STATE_OFF) {
            continue;
        }
        bool down = gSaveContext.timerDirections[id] == TIMER_COUNT_DOWN;
        int64_t limit = (int64_t)gSaveContext.timerTimeLimits[id];
        int64_t cur = (int64_t)gSaveContext.timerCurTimes[id];
        int64_t elapsed = std::clamp<int64_t>(down ? limit - cur : cur, 0, 36000000);
        sLastHudCs = elapsed;
        bool run = state == TIMER_STATE_COUNTING || state == TIMER_STATE_ALT_COUNTING ||
                   state == TIMER_STATE_POSTMAN_COUNTING;
        ev["timer"] = { { "id", id }, { "down", down }, { "limit", std::clamp<int64_t>(limit, 0, 36000000) },
                        { "elapsed", elapsed }, { "run", run } };
        return;
    }
}

void SendHud(PlayState* play) {
    Player* link = Link(play);
    json ev = MakeEvent(ev::kActHud);
    ev["key"] = sRunning->key;
    ev["score"] = gSaveContext.minigameScore;
    ev["hidden"] = gSaveContext.minigameHiddenScore;
    ev["status"] = std::min<int>(gSaveContext.minigameStatus, 3);
    ev["mstate"] = play->interfaceCtx.minigameState;
    ev["perfect"] = play->interfaceCtx.perfectLettersOn ? std::clamp<int>(play->interfaceCtx.perfectLettersType, 0, 16) : 0;
    ev["ammo"] = play->interfaceCtx.minigameAmmo;
    ev["b"] = play->bButtonAmmoPlusOne;
    ev["bomb"] = play->unk_1887E;
    ev["chu"] = play->unk_1887D;
    ev["arrows"] = (link->stateFlags3 & PLAYER_STATE3_400) != 0;
    ev["frozen"] = (link->stateFlags1 & PLAYER_STATE1_20) != 0;
    u16 sub = AudioSeq_GetActiveSeqId(SEQ_PLAYER_BGM_SUB);
    ev["sub"] = sub == NA_BGM_DISABLED ? -1 : (int)sub;
    ev["pos"] = { link->actor.world.pos.x, link->actor.world.pos.y, link->actor.world.pos.z };
    ev["rot"] = link->actor.shape.rot.y;
    AddTimer(ev);
    NetClient::Get().SendEvent(ev);
}

// The group comes along when this game goes to one of the minigame's entrances, or out of it while it runs.
void CheckFollow(PlayState* play) {
    bool starting = play->transitionTrigger == TRANS_TRIGGER_START;
    if (starting && !sPrevStarting) {
        if (sRunning != nullptr && sRunning->mode == ActivityMode::EachOwn &&
            !Activity_IsSpecialEntrance(*sRunning, play->nextEntrance)) {
            Director_ReportResult(*sRunning); // our own run is over
        }
        const ActivityDef* def = sRunning != nullptr ? sRunning : Activity_MinigameOfScene(play->sceneId);
        bool special = def != nullptr && Activity_IsSpecialEntrance(*def, play->nextEntrance);
        // Cada uno: the group comes along to the race's entrance, and each one leaves it when its own run ends
        if (def != nullptr && def->follow && (special || (sRunning != nullptr && def->mode != ActivityMode::EachOwn)) &&
            Group_AnyMateNear(kFollowDist)) {
            json ev = MakeEvent(ev::kFollow);
            ev["entrance"] = play->nextEntrance;
            ev["cs"] = gSaveContext.nextCutsceneIndex;
            ev["trans"] = play->transitionType;
            ev["key"] = def->key;
            NetClient::Get().SendEvent(ev);
            SPDLOG_INFO("[Coop] {}: the group comes along to entrance {:#x}", def->key, play->nextEntrance);
        }
    }
    sPrevStarting = starting;
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (!WorldSession_Active() || play == nullptr || !PoseCapture_InGameplay()) {
        return;
    }
    int64_t now = Group_NowMs();
    const ActivityDef* here = Activity_MinigameOfScene(play->sceneId);
    if (here != nullptr && !Guest_Joined() && Signals(play, *here)) {
        sLastSignalMs = now;
        if (sRunning != here) {
            End();
            Start(play, here);
        }
    } else if (sRunning != nullptr && (play->sceneId != sRunningScene || now - sLastSignalMs > kEndAfterMs)) {
        End();
    }
    if (sRunning != nullptr && gSaveContext.minigameStatus == MINIGAME_STATUS_ACTIVE) {
        sScored = true;
    }
    Player* link = Link(play);
    if (here != nullptr && link->talkActor != nullptr && link->talkActor->id == here->npcs[0] &&
        play->msgCtx.msgMode != MSGMODE_NONE) {
        sTalkedToNpcMs = now;
    }
    const ActivityDef* at = sRunning != nullptr ? sRunning : (here != nullptr ? here : QuestHere(play));
    std::string key = at != nullptr ? at->key : "";
    if (key != sHereKey) {
        sHereKey = key;
        if (sRunning == nullptr) {
            Send(at != nullptr ? "here" : "none", at);
        }
    }
    if (sRunning != nullptr && ++sFrames % kHudEvery == 0 && Group_AnyMateNear(0.f)) {
        SendHud(play);
    }
    CheckFollow(play);
}

void Reset() {
    sRunning = nullptr;
    sRunningScene = -1;
    sLastRan = nullptr;
    sHereKey = "-";
    sPrevStarting = false;
    sOwnRuns.clear();
    sResultKey.clear();
}

void RegisterDirector() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
}

} // namespace

const ActivityDef* Director_Running() {
    return sRunning;
}

const ActivityDef* Director_Recent() {
    if (sRunning != nullptr) {
        return sRunning;
    }
    return (sLastRan != nullptr && Group_NowMs() - sEndedMs < kRecentMs) ? sLastRan : nullptr;
}

bool Director_PinsActorId(int16_t actorId, int16_t scene) {
    const ActivityDef* def = Pinned(scene);
    return def != nullptr && def->scene == scene && (def->npcs[0] == actorId || Activity_Prop(*def, actorId) != nullptr);
}

bool Director_PinsFamily(const Actor* root) {
    if (root == nullptr || gPlayState == nullptr) {
        return false;
    }
    const ActivityDef* def = Pinned(gPlayState->sceneId);
    if (def == nullptr || def->scene != gPlayState->sceneId) {
        return false;
    }
    const ActivityProp* prop = Activity_Prop(*def, root->id);
    return def->npcs[0] == root->id || (prop != nullptr && prop->ours);
}

int64_t Director_LastOwnRunMs(const std::string& key) {
    auto it = sOwnRuns.find(key);
    return it == sOwnRuns.end() ? 0 : it->second;
}

void Director_ReportResult(const ActivityDef& def) {
    bool won = false;
    int32_t cs = 0;
    if (def.hooks == nullptr || def.hooks->result == nullptr || !def.hooks->result(&won, &cs)) {
        return;
    }
    int64_t now = Group_NowMs();
    if (sResultKey == def.key && now - sResultMs < kResultOnceMs) {
        return; // this run was told already
    }
    sResultKey = def.key;
    sResultMs = now;
    cs = won ? std::clamp<int32_t>(cs, 0, 36000000) : 0; // the clock of a lost race is the winner's
    json ev = MakeEvent(ev::kAct);
    ev["state"] = "result";
    ev["key"] = def.key;
    ev["name"] = def.name;
    ev["won"] = won;
    ev["cs"] = cs;
    NetClient::Get().SendEvent(ev);
    Chat_Add(ChatKind::Info, std::string(won ? "Has ganado " : "No has ganado ") + def.name +
                                 (cs > 0 ? " (" + Activity_TimeText(cs) + ")." : "."));
}

} // namespace coop::client

COOP_ON_EVENT(directorWelcome, coop::ev::kWelcome, [](const coop::json&) { coop::client::Reset(); });
COOP_ON_LOST(directorLost, [](const std::string&) { coop::client::Reset(); });
static RegisterShipInitFunc sDirectorInit(coop::client::RegisterDirector);

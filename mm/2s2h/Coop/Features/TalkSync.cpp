// [COOP] Dialogues seen together (spec §6). Whoever talks to a shared actor (an NPC, enemy or boss every game copies),
// or reads a minigame's text while it runs it, sends what its text box does ("talk": open, id, page, choice, close).
// The mates next to it (or everyone near a boss) open the same message in their own text box. Theirs only moves when
// the talker's does (VB_MSG_ADVANCE), the cursor of a choice is the talker's, and it closes with it. Boxes with number
// inputs (bank, lottery, bets, the Bombers' code) and the ocarina are never shown. Values in the text (a name,
// rupees) are the reader's own.
#include "TalkSync.h"

#include "Cinema.h"
#include "Ending.h"
#include "MessageVars.h"
#include "2s2h/Coop/Activities/Activities.h"
#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"
#include "2s2h/Coop/Group/Group.h"
#include "2s2h/Coop/Puppet/PuppetManager.h"
#include "2s2h/Coop/Room/Room.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

constexpr float kWatchDist = 1500.f;      // mates this close see our dialogues
constexpr float kSceneWatchDist = 3000.f; // a fighter's dialogue outside a boss's room reaches this far
constexpr int64_t kStaleMs = 30000;       // a mirrored box with no news for this long closes

bool TextMode(u8 mode) {
    return (mode >= MSGMODE_TEXT_START && mode <= MSGMODE_TEXT_DELAYED_BREAK) || mode == MSGMODE_TEXT_AWAIT_NEXT ||
           mode == MSGMODE_TEXT_DONE;
}

bool InputBox(u8 endType) {
    return endType >= TEXTBOX_ENDTYPE_INPUT_BANK && endType <= TEXTBOX_ENDTYPE_64;
}

Player* Link(PlayState* play) {
    return (Player*)play->actorCtx.actorLists[ACTORCAT_PLAYER].first;
}

bool AnyoneElseHere(PlayState* play) {
    for (const auto& [id, p] : Session_Players()) {
        if (p.scene == play->sceneId) {
            return true;
        }
    }
    return false;
}

// ---- The one who talks ----

struct Sent {
    bool open = false;
    uint16_t id = 0;
    int page = 0;
    int choice = -1;
    const char* scope = "group";
};

// ---- The one who reads along ----

struct Mirror {
    bool active = false;
    uint8_t from = 0;
    uint16_t id = 0;
    int page = 0;    // pauses the talker went past in this message
    int shown = 0;   // pauses we went past
    int choice = 0;
    int64_t lastMs = 0;
    bool advanced = false; // one pause per frame at most
};

Sent sSent;
u8 sPrevMode = MSGMODE_NONE;
Mirror sMirror;

// The mirrored box is the one open: it has no actor (a conversation of our own has one).
bool OurBox(PlayState* play) {
    return sMirror.active && play->msgCtx.msgMode != MSGMODE_NONE && play->msgCtx.talkActor == nullptr;
}

void SendOp(const char* op) {
    json ev = MakeEvent(ev::kTalk);
    ev["op"] = op;
    std::string o = op;
    if (o == "open" || o == "id") {
        ev["id"] = sSent.id;
        ev["vars"] = MessageVars_Capture(); // our name, points, amounts: the mirrored text shows them
    } else if (o == "page") {
        ev["page"] = std::min(sSent.page, 1000);
    } else if (o == "choice") {
        ev["choice"] = std::clamp(sSent.choice, 0, 2);
    }
    ev["scope"] = sSent.scope;
    NetClient::Get().SendEvent(ev);
}

// Who hears this conversation (nullptr: nobody).
const char* ScopeOf(PlayState* play) {
    Actor* who = play->msgCtx.talkActor;
    if (who != nullptr && ActorRegistry_Get(who) != nullptr) {
        bool fighter = who->category == ACTORCAT_BOSS || who->category == ACTORCAT_ENEMY;
        if (fighter || BossArena_Is(play->sceneId)) {
            return AnyoneElseHere(play) ? "scene" : nullptr;
        }
        return Mates_AnyNear(kWatchDist) ? "group" : nullptr; // (group or room mates: "group" scope)
    }
    const ActivityDef* d = Director_Running();
    if (who == nullptr && d != nullptr && d->mode != ActivityMode::EachOwn && Mates_AnyNear(kWatchDist)) {
        return "group";
    }
    return nullptr;
}

void TalkerFrame(PlayState* play) {
    MessageContext* msg = &play->msgCtx;
    bool text = TextMode(msg->msgMode) && !InputBox(msg->textboxEndType) && !sMirror.active;
    if (!sSent.open) {
        const char* scope = text ? ScopeOf(play) : nullptr;
        if (scope != nullptr) {
            sSent = Sent{ true, msg->currentTextId, 0, -1, scope };
            SendOp("open");
        }
    } else if (!text) {
        SendOp("close");
        sSent = Sent{};
    } else if (msg->currentTextId != sSent.id) {
        sSent.id = msg->currentTextId;
        sSent.page = 0;
        sSent.choice = -1;
        SendOp("id");
    } else if ((sPrevMode == MSGMODE_TEXT_AWAIT_INPUT || sPrevMode == MSGMODE_TEXT_AWAIT_NEXT) &&
               msg->msgMode != sPrevMode) {
        sSent.page++;
        SendOp("page");
    }
    if (sSent.open && Message_GetState(msg) == TEXT_STATE_CHOICE && msg->choiceIndex != sSent.choice) {
        sSent.choice = msg->choiceIndex;
        SendOp("choice");
    }
    sPrevMode = msg->msgMode;
}

bool Hears(PlayState* play, uint8_t from, const std::string& scope) {
    if (!Group_OptDialogues() || !WorldSession_Active() || EndingMode_Active()) {
        return false;
    }
    if (scope == "scene") {
        if (BossArena_Is(play->sceneId)) {
            return true;
        }
        Actor* p = PuppetManager_Actor(from);
        return p != nullptr && Actor_WorldDistXYZToActor(&Link(play)->actor, p) < kSceneWatchDist;
    }
    return Mates_Is(from) &&
           (Mates_Near(from, kWatchDist) || Guest_Director() == from || Cinema_WatchingFrom(from));
}

bool CanOpen(PlayState* play) {
    Player* link = Link(play);
    return play->msgCtx.msgMode == MSGMODE_NONE && play->transitionTrigger == TRANS_TRIGGER_OFF &&
           play->transitionMode == TRANS_MODE_OFF && !IS_PAUSED(&play->pauseCtx) &&
           play->gameOverCtx.state == GAMEOVER_INACTIVE && !(link->stateFlags1 & PLAYER_STATE1_DEAD) &&
           (!Play_InCsMode(play) || Cinema_Watching());
}

void CloseMirror(PlayState* play) {
    if (play != nullptr && OurBox(play)) {
        Message_CloseTextbox(play);
    }
    sMirror = Mirror{};
    MessageVars_Clear();
}

void OnTalk(const json& ev) {
    PlayState* play = gPlayState;
    uint8_t from = (uint8_t)GetInt(ev, "from");
    if (play == nullptr || from == 0 || from == Session_LocalId()) {
        return;
    }
    std::string op = GetString(ev, "op");
    if (op == "open") {
        if (sMirror.active || !Hears(play, from, GetString(ev, "scope", "group")) || !CanOpen(play)) {
            return;
        }
        uint16_t id = (uint16_t)GetInt(ev, "id");
        MessageVars_Set(GetString(ev, "vars"));
        Message_StartTextbox(play, id, NULL);
        sMirror = Mirror{ true, from, id, 0, 0, 0, Group_NowMs(), false };
        SPDLOG_INFO("[Coop] Showing the dialogue of player {} (text {:#x})", (int)from, id);
        return;
    }
    if (!sMirror.active || sMirror.from != from) {
        return;
    }
    sMirror.lastMs = Group_NowMs();
    if (op == "id") {
        uint16_t id = (uint16_t)GetInt(ev, "id");
        if (!OurBox(play)) {
            sMirror = Mirror{}; // our own conversation took the box
            MessageVars_Clear();
            return;
        }
        MessageVars_Set(GetString(ev, "vars"));
        if (play->msgCtx.currentTextId != id) {
            Message_ContinueTextbox(play, id);
        }
        sMirror.id = id;
        sMirror.page = 0;
        sMirror.shown = 0;
        sMirror.choice = 0;
    } else if (op == "page") {
        sMirror.page = std::max(sMirror.page, (int)std::clamp<int64_t>(GetInt(ev, "page"), 0, 1000));
    } else if (op == "choice") {
        sMirror.choice = (int)std::clamp<int64_t>(GetInt(ev, "choice"), 0, 2);
    } else if (op == "close") {
        CloseMirror(play);
    }
}

void WatcherFrame(PlayState* play) {
    if (!sMirror.active) {
        return;
    }
    if (!OurBox(play)) {
        sMirror = Mirror{}; // closed, or our own conversation took the box
        return;
    }
    const RemotePlayer* talker = Session_FindPlayer(sMirror.from);
    bool gone = talker == nullptr || talker->scene != play->sceneId || Group_NowMs() - sMirror.lastMs > kStaleMs;
    if (gone || InputBox(play->msgCtx.textboxEndType) || !Group_OptDialogues()) {
        CloseMirror(play);
        return;
    }
    if (Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) {
        play->msgCtx.choiceIndex = (u8)sMirror.choice;
    }
}

void FrameEnd() {
    PlayState* play = gPlayState;
    if (play == nullptr || !WorldSession_Active() || EndingMode_Active()) {
        if (sSent.open) {
            SendOp("close");
        }
        sSent = Sent{};
        sMirror = Mirror{};
        return;
    }
    WatcherFrame(play);
    TalkerFrame(play);
}

void RegisterTalkSync() {
    COND_HOOK(OnGameStateMainStart, true, []() { sMirror.advanced = false; });
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    // the choice's cursor drawn this frame is the talker's (our stick moves it during the update)
    COND_HOOK(OnPlayDrawWorldStart, true, []() {
        PlayState* play = gPlayState;
        if (play != nullptr && OurBox(play) && Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) {
            play->msgCtx.choiceIndex = (u8)sMirror.choice;
        }
    });
    COND_HOOK(OnPlayDestroy, true, []() {
        sMirror = Mirror{};
        sSent = Sent{};
        sPrevMode = MSGMODE_NONE;
    });
    // Our mirrored box only moves when the talker's did: one pause each time it went past one
    COND_VB_SHOULD(VB_MSG_ADVANCE, true, {
        PlayState* play = gPlayState;
        if (play == nullptr || !OurBox(play)) {
            return;
        }
        MessageContext* msg = &play->msgCtx;
        bool atPause = (msg->msgMode == MSGMODE_TEXT_AWAIT_INPUT || msg->msgMode == MSGMODE_TEXT_AWAIT_NEXT) &&
                       msg->currentTextId == sMirror.id;
        if (atPause && sMirror.shown < sMirror.page && !sMirror.advanced) {
            sMirror.shown++;
            sMirror.advanced = true;
            *should = true;
        } else {
            *should = false;
        }
    });
}

} // namespace

bool TalkSync_Mirroring() {
    return sMirror.active;
}

uint16_t TalkSync_MirroredTextId() {
    return sMirror.active ? sMirror.id : 0;
}

} // namespace coop::client

COOP_ON_EVENT(talkSyncEvent, coop::ev::kTalk, coop::client::OnTalk);
COOP_ON_LOST(talkSyncLost, [](const std::string&) {
    coop::client::sMirror = coop::client::Mirror{};
    coop::client::sSent = coop::client::Sent{};
});
static RegisterShipInitFunc sTalkSyncInit(coop::client::RegisterTalkSync);

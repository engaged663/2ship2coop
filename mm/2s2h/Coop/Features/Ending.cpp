// [COOP] The end of the game is reached together ("ending" events; server.json "endingForAll"). Stages:
//   1 the Clock Tower's rooftop: whoever climbs it takes everyone there (the countdown: RooftopTimer.cpp);
//   2 Majora's lair: Oath to Order on the rooftop (with the four remains) goes straight to the fight, for everyone;
//     the Moon and its children are skipped (whoever has the 20 masks gets the Fierce Deity's Mask right there);
//   3 Majora defeated: the others watch her death through the camera of the game that runs her (Cinema.cpp);
//   4 the ending: everyone goes with the one who starts it and it plays synchronized (EndingMode.cpp).
// A player is taken as soon as the game lets them move (no scene change, pause or game over): the rooftop waits for
// a text or a cutscene up to kForceAfterMs, the fight and the ending never wait.
#include "Ending.h"

#include "2s2h/Coop/Chat/ChatModel.h"
#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Puppet/PoseCapture.h"
#include "2s2h/Coop/World/WorldSession.h"

#include "common/Protocol.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <chrono>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64horse.h"
s32 Cutscene_CountNormalMasks(void); // z_demo.c
}

namespace coop::client {

namespace {

constexpr int kStageRooftop = 1;
constexpr int kStageLair = 2;
constexpr int kStageDefeated = 3;
constexpr int kStageEnding = 4;
constexpr u16 kEndingCutscene = 0xFFF7; // the ending in Termina Field (Boss_07 sets it when Majora dies)
constexpr s16 kOathCutscene = 12;       // the rooftop's cutscene of the giants (En_Time_Tag, Oath to Order)
constexpr int64_t kForceAfterMs = 8000;

int sLastStage = 0; // the stage of where we were last frame
int sPending = 0;   // a stage another player reached (or our Oath to Order) that we must go to
int64_t sPendingSinceMs = 0;
bool sDefeated = false; // Majora was defeated (here or in another game)

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// The stage of where this game is (or is going right now).
int StageHere(PlayState* play) {
    bool toEnding = play->transitionTrigger != TRANS_TRIGGER_OFF &&
                    play->nextEntrance == ENTRANCE(TERMINA_FIELD, 0) &&
                    gSaveContext.nextCutsceneIndex == kEndingCutscene;
    if (EndingMode_Active() || toEnding || gSaveContext.gameMode == GAMEMODE_END_CREDITS) {
        return kStageEnding;
    }
    if (play->sceneId == SCENE_LAST_BS) {
        return sDefeated ? kStageDefeated : kStageLair;
    }
    return play->sceneId == SCENE_OKUJOU ? kStageRooftop : 0;
}

void Announce(int stage) {
    json ev = MakeEvent(ev::kEnding);
    ev["stage"] = stage;
    NetClient::Get().SendEvent(ev);
}

void Go(int stage) {
    PlayState* play = gPlayState;
    if (play->msgCtx.msgMode != MSGMODE_NONE) {
        Message_CloseTextbox(play);
    }
    gSaveContext.nextCutsceneIndex = 0;
    switch (stage) {
        case kStageRooftop:
            play->nextEntrance = ENTRANCE(CLOCK_TOWER_ROOFTOP, 0);
            break;
        case kStageLair:
            RooftopTimer_Stop();
            play->nextEntrance = ENTRANCE(MAJORAS_LAIR, 0);
            break;
        default:
            play->nextEntrance = ENTRANCE(TERMINA_FIELD, 0);
            gSaveContext.nextCutsceneIndex = kEndingCutscene;
            EndingMode_Begin();
            break;
    }
    gHorseIsMounted = false; // on foot: the game must not bring Epona along
    gSaveContext.respawnFlag = 0;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = stage == kStageRooftop ? TRANS_TYPE_FADE_BLACK : TRANS_TYPE_FADE_WHITE;
    gSaveContext.nextTransitionType = play->transitionType;
}

// Link can be taken away now (force: a text or a cutscene is not waited for).
bool CanGo(PlayState* play, bool force) {
    Player* player = GET_PLAYER(play);
    if (play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF ||
        IS_PAUSED(&play->pauseCtx) || (player->stateFlags1 & PLAYER_STATE1_DEAD) ||
        play->gameOverCtx.state != GAMEOVER_INACTIVE) {
        return false;
    }
    if (force) {
        return true;
    }
    return play->msgCtx.msgMode == MSGMODE_NONE && !Play_InCsMode(play) &&
           CutsceneManager_GetCurrentCsId() == CS_ID_NONE;
}

// The kids of the Moon give the Fierce Deity's Mask to whoever has the 20 masks: without the Moon, it comes here.
void GiveFierceDeityIfEarned() {
    if (Cutscene_CountNormalMasks() < 20 || INV_CONTENT(ITEM_MASK_FIERCE_DEITY) == ITEM_MASK_FIERCE_DEITY) {
        return;
    }
    Item_Give(gPlayState, ITEM_MASK_FIERCE_DEITY);
    SET_WEEKEVENTREG(WEEKEVENTREG_84_20);
    Chat_Add(ChatKind::Ok, "Tenéis las 20 máscaras: la Máscara de la Fiera Deidad es vuestra.");
}

// Oath to Order with the four remains on the rooftop: the giants hold the moon and everyone goes to Majora.
void OathToOrder() {
    SET_WEEKEVENTREG(WEEKEVENTREG_25_02); // giants called to the tower
    SET_WEEKEVENTREG(WEEKEVENTREG_93_04); // watched the giants stop the moon
    GiveFierceDeityIfEarned();
    RooftopTimer_Stop();
    Announce(kStageLair);
    sPending = kStageLair;
    sPendingSinceMs = NowMs();
    Chat_Add(ChatKind::Info, "Los gigantes detienen la luna. ¡A por Majora!");
}

void FrameEnd() {
    if (!WorldSession_Active() || gPlayState == nullptr || !PoseCapture_InGameplay()) {
        return;
    }
    PlayState* play = gPlayState;
    int here = StageHere(play);
    if (here == kStageEnding && !EndingMode_Active()) {
        EndingMode_Begin(); // Majora died in this game: the ending starts here
    }
    if (here > sLastStage && here != kStageDefeated) {
        Announce(here); // we got there (on our own or taken): the others follow
    }
    sLastStage = here;
    if (sPending <= here) {
        sPending = 0;
        return;
    }
    bool force = sPending >= kStageLair || NowMs() - sPendingSinceMs >= kForceAfterMs;
    if (CanGo(play, force)) {
        Go(sPending);
        sPending = 0;
    }
}

void OnEnding(const json& ev) {
    int stage = (int)GetInt(ev, "stage");
    if (!WorldSession_Active() || gPlayState == nullptr || stage < kStageRooftop || stage > kStageEnding ||
        EndingMode_Active()) {
        return;
    }
    std::string nick = GetString(ev, "nick");
    if (nick.empty()) {
        nick = "La partida del servidor";
    }
    if (stage == kStageDefeated) {
        if (!sDefeated) {
            sDefeated = true;
            Chat_Add(ChatKind::Ok, "¡" + nick + " ha derrotado a Majora!");
        }
        return;
    }
    if (stage <= StageHere(gPlayState) || stage <= sPending) {
        return;
    }
    sPending = stage;
    sPendingSinceMs = NowMs();
    Chat_Add(ChatKind::Info, stage == kStageRooftop ? nick + " ha subido a la torre del reloj: vais todos allí."
                             : stage == kStageLair  ? nick + " ha tocado la Canción del Juramento: ¡todos contra Majora!"
                                                    : nick + " empieza el final del juego: lo veis todos juntos.");
}

void Reset() {
    sLastStage = 0;
    sPending = 0;
    sDefeated = false;
}

void RegisterEnding() {
    COND_HOOK(OnGameStateMainFinish, true, FrameEnd);
    // Majora's Wrath died in this game (it runs her): everyone watches (Cinema.cpp), then the ending
    COND_HOOK(OnBossDefeated, true, [](s16 actorId) {
        if (actorId == ACTOR_BOSS_07 && WorldSession_Active() && !EndingMode_Active() && !sDefeated) {
            sDefeated = true;
            Announce(kStageDefeated);
        }
    });
    // Oath to Order on the rooftop: straight to Majora (SkipStoppingMoonCutscene.cpp stands aside in the world).
    // Without the four remains the original goes on (the giants cannot hold the moon).
    COND_VB_SHOULD(VB_START_CUTSCENE, true, {
        s16* csId = va_arg(args, s16*);
        if (!WorldSession_Active() || EndingMode_Active() || gPlayState == nullptr ||
            gPlayState->sceneId != SCENE_OKUJOU || *csId != kOathCutscene) {
            return;
        }
        if (!GameInteractor_Should(VB_MEET_MOON_REQUIREMENTS,
                                   CHECK_QUEST_ITEM(QUEST_REMAINS_ODOLWA) && CHECK_QUEST_ITEM(QUEST_REMAINS_GOHT) &&
                                       CHECK_QUEST_ITEM(QUEST_REMAINS_GYORG) &&
                                       CHECK_QUEST_ITEM(QUEST_REMAINS_TWINMOLD))) {
            return;
        }
        *should = false;
        OathToOrder();
    });
}

} // namespace

bool Ending_StopsTime() {
    return EndingMode_Active() || (gPlayState != nullptr && StageHere(gPlayState) >= kStageLair);
}

COOP_ON_EVENT(endingEvent, coop::ev::kEnding, OnEnding);
COOP_ON_EVENT(endingWelcome, coop::ev::kWelcome, [](const json&) { Reset(); });
COOP_ON_EVENT(endingWorld, coop::ev::kWorldFull, [](const json&) { Reset(); });
COOP_ON_LOST(endingLost, [](const std::string&) { Reset(); });
static RegisterShipInitFunc sEndingInit(RegisterEnding);

} // namespace coop::client

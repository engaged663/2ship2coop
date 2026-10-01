// [COOP] The pause menu of the server's world stops nothing ("live menu"). TimeNeverStops.cpp keeps the world running
// behind the menu; this file makes the menu a plain layer over the game: Link runs too, with a controller nobody
// touches (the menu has the real one), the world is drawn behind the menu, the sound and the pace of the game do not
// change, and the menu closes by itself when the game needs Link or the screen (the rules: coop/common/LiveMenu.h).
// The engine asks these functions (z_play.c, z_kaleido_setup.c, z_kaleido_scope_call.c, z_message.c and the "is it
// paused?" checks, all marked [COOP]). The game over screens are not that menu: they stay as they were.
// Off switch: gCoop.LiveMenu = 0 (the menu over a still picture and Link waiting, as before).
#include "CoopEngine.h"

#include "common/LiveMenu.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <cstring>

extern "C" {
#include "functions.h"
#include "regs.h"
#include "variables.h"
#include "overlays/kaleido_scope/ovl_kaleido_scope/z_kaleido_scope.h"
}

static_assert((int)coop::LiveMenuClose::No == COOP_MENU_CLOSE_NO &&
                  (int)coop::LiveMenuClose::DropText == COOP_MENU_CLOSE_DROP_TEXT &&
                  (int)coop::LiveMenuClose::KeepText == COOP_MENU_CLOSE_KEEP_TEXT,
              "COOP_MENU_CLOSE_* out of date with coop::LiveMenuClose");

namespace {

coop::LiveMenuWatch sWatch;
Input sRealInput;     // controller 1, while what is behind the menu runs with a neutral one
bool sNeutral = false;
uint32_t sFrames = 0; // frames of the game this menu has been open

// The player's choice (gCoop.LiveMenu) as it was when this menu opened: switched with a menu open, it waits for the
// next one (a menu that opened one way and goes on the other would run the world at the menu's pace, or muted)
bool sChoice = false;
bool sChoiceMade = false;

// The pause menu (or the owls' map) is open, and it is the kind that stops nothing
bool MenuOpen(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;

    if (pauseCtx->state == PAUSE_STATE_OFF) {
        sChoiceMade = false;
        return false;
    }
    if (!sChoiceMade) {
        sChoiceMade = true;
        sChoice = CVarGetInteger("gCoop.LiveMenu", 1) != 0;
    }
    return sChoice && !IS_PAUSE_STATE_GAMEOVER(pauseCtx) && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

// The owls' map of the Song of Soaring (a pause menu too), from the moment the game asks for it
bool OwlMap(const PauseContext* pauseCtx) {
    return pauseCtx->state >= PAUSE_STATE_OWL_WARP_0 && pauseCtx->state <= PAUSE_STATE_OWL_WARP_6;
}

// The menu is past its own first step (KaleidoScope_Update's OPENING_2 / OWL_WARP_2): it keeps the buttons and the
// HUD of before, and gives them back in its last step (UNPAUSE_CLOSE)
bool SetUp(const PauseContext* pauseCtx) {
    return (pauseCtx->state >= PAUSE_STATE_OPENING_3 && pauseCtx->state <= PAUSE_STATE_SAVEPROMPT) ||
           (pauseCtx->state >= PAUSE_STATE_OWL_WARP_3 && pauseCtx->state <= PAUSE_STATE_UNPAUSE_SETUP);
}

// Already on its way out (its closing sound has played)
bool Leaving(const PauseContext* pauseCtx) {
    return pauseCtx->state == PAUSE_STATE_UNPAUSE_SETUP || pauseCtx->state == PAUSE_STATE_OWL_WARP_6 ||
           (pauseCtx->state == PAUSE_STATE_SAVEPROMPT && (pauseCtx->savePromptState == PAUSE_SAVEPROMPT_STATE_3 ||
                                                          pauseCtx->savePromptState == PAUSE_SAVEPROMPT_STATE_7));
}

coop::LiveMenuView Look(PlayState* play) {
    coop::LiveMenuView v;
    v.transition = play->transitionTrigger != TRANS_TRIGGER_OFF || play->transitionMode != TRANS_MODE_OFF;
    v.gameOver = play->gameOverCtx.state != GAMEOVER_INACTIVE;
    v.taken = Play_InCsMode(play);
    v.textOpen = play->msgCtx.msgMode != MSGMODE_NONE;
    v.textId = play->msgCtx.currentTextId;
    return v;
}

// The menu closes now, with no closing animation: the game needs Link or the screen. keepText: the open text box is
// the game's, not the menu's.
void ForceClose(PlayState* play, bool keepText) {
    PauseContext* pauseCtx = &play->pauseCtx;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    MessageContext* msgCtx = &play->msgCtx;

    if (pauseCtx->state == PAUSE_STATE_OFF || pauseCtx->state == PAUSE_STATE_UNPAUSE_CLOSE) {
        return; // closed, or closing in the menu's next update
    }

    // The menu's own text box (an item's description, the question of the owls' map, a page of the notebook) goes
    // with it, the way the notebook drops its own when it closes
    if (!keepText && msgCtx->msgMode != MSGMODE_NONE) {
        msgCtx->msgLength = 0;
        msgCtx->msgMode = MSGMODE_NONE;
        msgCtx->currentTextId = 0;
        msgCtx->stateTimer = 0;
    }
    pauseCtx->itemDescriptionOn = false; // a text box that stays is read as any other

    if (pauseCtx->state >= PAUSE_STATE_OPENING_0 && pauseCtx->state <= PAUSE_STATE_OPENING_4) {
        // The menu opens by turning to its page from the one on its right (func_800F4A10) and was still on that
        // one: it stays on the page it was turning to (of the four: item, map, quest, mask)
        pauseCtx->pageIndex = (pauseCtx->pageIndex + PAUSE_MASK) % (PAUSE_MASK + 1);
    }

    if (OwlMap(pauseCtx)) {
        // As backing out of the map; a destination already chosen (the map was closing) stays chosen
        if (pauseCtx->state != PAUSE_STATE_OWL_WARP_6) {
            msgCtx->ocarinaMode = OCARINA_MODE_END;
            gSaveContext.prevHudVisibility = HUD_VISIBILITY_ALL;
        }
        pauseCtx->pageIndex = pauseCtx->unk_2C8;
        pauseCtx->cursorPoint[PAUSE_WORLD_MAP] = pauseCtx->unk_2CA;
    }

    // A song the Quest Status page was playing
    if (pauseCtx->state == PAUSE_STATE_MAIN &&
        (pauseCtx->mainState == PAUSE_MAIN_STATE_SONG_PLAYBACK || IS_PAUSE_MAIN_STATE_SONG_PROMPT(pauseCtx) ||
         pauseCtx->mainState == PAUSE_MAIN_STATE_IDLE_CURSOR_ON_SONG ||
         pauseCtx->mainState == PAUSE_MAIN_STATE_SONG_PLAYBACK_INIT)) {
        AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
    }

    if (!Leaving(pauseCtx)) {
        Audio_PlaySfx_PauseMenuOpenOrClose(SFX_PAUSE_MENU_CLOSE);
    }

    if (SetUp(pauseCtx)) {
        // Where the closing animation ends (KaleidoScope_Update, UNPAUSE_SETUP); the menu's last step, in its next
        // update, gives the buttons and the HUD back
        pauseCtx->debugEditor = DEBUG_EDITOR_NONE;
        pauseCtx->itemPageRoll = pauseCtx->mapPageRoll = pauseCtx->questPageRoll = pauseCtx->maskPageRoll = 160.0f;
        pauseCtx->alpha = 0;
        pauseCtx->namedItem = PAUSE_ITEM_NONE;
        pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
        interfaceCtx->startAlpha = 0;
        Interface_SetAButtonDoAction(play, DO_ACTION_NONE);
        pauseCtx->state = PAUSE_STATE_UNPAUSE_CLOSE;
    } else {
        // Still opening: it had taken nothing yet, there is nothing to give back
        if (OwlMap(pauseCtx)) {
            interfaceCtx->bButtonInterfaceDoActionActive = interfaceCtx->bButtonInterfaceDoAction = 0;
        }
        pauseCtx->state = PAUSE_STATE_OFF;
        if (R_PAUSE_BG_PRERENDER_STATE != PAUSE_BG_PRERENDER_OFF) {
            R_PAUSE_BG_PRERENDER_STATE = PAUSE_BG_PRERENDER_UNK4; // Play_DrawMain turns it off
        }
        GameState_SetFramerateDivisor(&play->state, 3);
        Audio_SetPauseState(false);
    }
}

} // namespace

extern "C" s32 Coop_PauseLive(PlayState* play) {
    return (MenuOpen(play) && Coop_InWorld()) ? 1 : 0;
}

extern "C" void Coop_LiveMenuClose(PlayState* play) {
    // Not Coop_PauseLive: with a new world waiting this game is not "in the world" any more (WorldSession_Active)
    if (MenuOpen(play)) {
        ForceClose(play, false);
    }
}

// The game leaves the song's result written when the owls' map opens (OCARINA_MODE_END, until a destination is chosen)
// and counts on Link not reading it before the map closes (Player_Action_63): there Link waits, as in the original.
extern "C" s32 Coop_LiveMenuHoldsLink(PlayState* play) {
    return OwlMap(&play->pauseCtx) ? 1 : 0;
}

extern "C" s32 Coop_LiveMenuBegin(PlayState* play) {
    if (!Coop_PauseLive(play)) {
        sWatch.Closed();
        sFrames = 0;
        return 0;
    }
    if (!sWatch.IsOpen()) {
        sWatch.Opened(Look(play));
    }
    return 1;
}

extern "C" void Coop_LiveMenuInput(PlayState* play, s32 neutral) {
    Input* input = CONTROLLER1(&play->state);

    if (neutral && !sNeutral) {
        sRealInput = *input;
        memset(input, 0, sizeof(Input));
        sNeutral = true;
    } else if (!neutral && sNeutral) {
        *input = sRealInput;
        sNeutral = false;
    }
}

extern "C" s32 Coop_LiveMenuAfterWorld(PlayState* play) {
    coop::LiveMenuClose close = sWatch.AfterWorld(Look(play));

    if (close != coop::LiveMenuClose::No) {
        ForceClose(play, close == coop::LiveMenuClose::KeepText);
    }
    return (s32)close;
}

extern "C" void Coop_LiveMenuOwnText(PlayState* play) {
    if (sWatch.IsOpen() && play->pauseCtx.state != PAUSE_STATE_OFF &&
        play->pauseCtx.state != PAUSE_STATE_UNPAUSE_CLOSE) {
        sWatch.AfterMenu(Look(play));
    }
}

extern "C" void Coop_LiveMenuUpdate(PlayState* play) {
    Input* input = CONTROLLER1(&play->state);
    auto press = input->press.button;
    s32 steps = coop::LiveMenu_Steps(sFrames++);

    for (s32 i = 0; i < steps && IS_PAUSED(&play->pauseCtx); i++) {
        if (i > 0) {
            input->press.button = 0; // a press counts once
        }
        KaleidoScopeCall_Update(play);
    }
    input->press.button = press;
    Coop_LiveMenuOwnText(play);
}

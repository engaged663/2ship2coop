#include "z64pause_menu.h"

#include "z64.h"
#include "z64shrink_window.h"
#include "2s2h/Coop/Actors/CoopEngine.h" // [COOP]

void (*sKaleidoScopeUpdateFunc)(PlayState* play);
void (*sKaleidoScopeDrawFunc)(PlayState* play);

extern void KaleidoScope_Update(PlayState* play);
extern void KaleidoScope_Draw(PlayState* play);

void KaleidoScopeCall_LoadPlayer(void) {
    KaleidoMgrOverlay* playerActorOvl = &gKaleidoMgrOverlayTable[KALEIDO_OVL_PLAYER_ACTOR];

    if (gKaleidoMgrCurOvl != playerActorOvl) {
        if (gKaleidoMgrCurOvl != NULL) {
            KaleidoManager_ClearOvl(gKaleidoMgrCurOvl);
        }

        KaleidoManager_LoadOvl(playerActorOvl);
    }
}

void KaleidoScopeCall_Init(PlayState* play) {
    sKaleidoScopeUpdateFunc = KaleidoManager_GetRamAddr(KaleidoScope_Update);
    sKaleidoScopeDrawFunc = KaleidoManager_GetRamAddr(KaleidoScope_Draw);
    KaleidoSetup_Init(play);
}

void KaleidoScopeCall_Destroy(PlayState* play) {
    KaleidoSetup_Destroy(play);
}

void KaleidoScopeCall_Update(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    KaleidoMgrOverlay* kaleidoScopeOvl = &gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];

    if (!IS_PAUSED(&play->pauseCtx)) {
        return;
    }

    if ((pauseCtx->state == PAUSE_STATE_OPENING_0) || (pauseCtx->state == PAUSE_STATE_OWL_WARP_0)) {
        // [COOP] The camera of the world running behind the menu may keep asking for its black bars (a target
        // locked on): that menu does not wait for them to go, it is drawn uncut over them (Play_PostWorldDraw)
        if ((ShrinkWindow_Letterbox_GetSize() == 0) || Coop_PauseLive(play)) {
            R_PAUSE_BG_PRERENDER_STATE = PAUSE_BG_PRERENDER_SETUP;
            pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
            pauseCtx->savePromptState = PAUSE_SAVEPROMPT_STATE_APPEARING;
            pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
        }
    } else if (pauseCtx->state == PAUSE_STATE_GAMEOVER_0) {
        R_PAUSE_BG_PRERENDER_STATE = PAUSE_BG_PRERENDER_SETUP;
        pauseCtx->mainState = PAUSE_MAIN_STATE_IDLE;
        pauseCtx->savePromptState = PAUSE_SAVEPROMPT_STATE_APPEARING;
        pauseCtx->state = (pauseCtx->state & 0xFFFF) + 1;
    } else if ((pauseCtx->state == PAUSE_STATE_OPENING_1) || (pauseCtx->state == PAUSE_STATE_GAMEOVER_1) ||
               (pauseCtx->state == PAUSE_STATE_OWL_WARP_1)) {
        if (R_PAUSE_BG_PRERENDER_STATE == PAUSE_BG_PRERENDER_READY) {
            pauseCtx->state++;
        }
    } else if (pauseCtx->state != PAUSE_STATE_OFF) {
        if (gKaleidoMgrCurOvl != kaleidoScopeOvl) {
            if (gKaleidoMgrCurOvl != NULL) {
                KaleidoManager_ClearOvl(gKaleidoMgrCurOvl);
            }

            KaleidoManager_LoadOvl(kaleidoScopeOvl);
        }

        if (gKaleidoMgrCurOvl == kaleidoScopeOvl) {
            sKaleidoScopeUpdateFunc(play);

            if (!IS_PAUSED(&play->pauseCtx)) {
                KaleidoManager_ClearOvl(kaleidoScopeOvl);
                KaleidoScopeCall_LoadPlayer();
            }
        }
    }
}

void KaleidoScopeCall_Draw(PlayState* play) {
    KaleidoMgrOverlay* kaleidoScopeOvl = &gKaleidoMgrOverlayTable[KALEIDO_OVL_KALEIDO_SCOPE];

    if (R_PAUSE_BG_PRERENDER_STATE == PAUSE_BG_PRERENDER_READY) {
        if (((play->pauseCtx.state >= PAUSE_STATE_OPENING_3) && (play->pauseCtx.state <= PAUSE_STATE_SAVEPROMPT)) ||
            ((play->pauseCtx.state >= PAUSE_STATE_GAMEOVER_3) && (play->pauseCtx.state <= PAUSE_STATE_UNPAUSE_SETUP))) {
            // [COOP] Link was just drawn behind the menu of the server's world, which made his code "the one loaded"
            // (KaleidoScopeCall_LoadPlayer): on the N64 the menu's and his share their memory. Here both always are,
            // so that menu is drawn anyway, once its first update has run (the owls' map is not ready before
            // OWL_WARP_3; the original left those states out by the menu's code not being loaded yet).
            if ((gKaleidoMgrCurOvl == kaleidoScopeOvl) ||
                (Coop_PauseLive(play) && !((play->pauseCtx.state >= PAUSE_STATE_OWL_WARP_0) &&
                                           (play->pauseCtx.state <= PAUSE_STATE_OWL_WARP_2)))) {
                sKaleidoScopeDrawFunc(play);
            }
        }
    }
}

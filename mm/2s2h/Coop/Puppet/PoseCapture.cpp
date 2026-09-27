#include "PoseCapture.h"

#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"

#include "common/PlayerState.h"

#include "2s2h/GameInteractor/GameInteractor.h"

#include <spdlog/spdlog.h>
#include "2s2h/ShipInit.hpp"

extern "C" {
#include "functions.h"
#include "variables.h"
}

static_assert(coop::kPoseJoints == PLAYER_LIMB_MAX, "PlayerState joints must match the Player skeleton");
// coop/common validates received poses with these limits: they must match the engine tables.
namespace limits = coop::pose_limits;
static_assert(limits::kFormCount == PLAYER_FORM_MAX && limits::kMaskCount == PLAYER_MASK_MAX &&
              limits::kShieldCount == PLAYER_SHIELD_MAX && limits::kModelGroupCount == PLAYER_MODELGROUP_MAX &&
              limits::kFaceCount == PLAYER_FACE_MAX && limits::kSwordMaxDrawn == EQUIP_VALUE_SWORD_GILDED &&
              limits::kItemActionMin == PLAYER_IA_MINUS1 && limits::kItemActionCount == PLAYER_IA_MAX &&
              limits::kEntranceScenes == ENTR_SCENE_MAX,
              "coop::pose_limits out of date with the engine enums");
static_assert(limits::kStateFlags1 ==
                  (u32)(PLAYER_STATE1_8000000 | PLAYER_STATE1_400000 | PLAYER_STATE1_ZORA_BOOMERANG_THROWN) &&
                  limits::kStateFlags2 == (u32)PLAYER_STATE2_20000000 &&
                  limits::kStateFlags3 == (u32)(PLAYER_STATE3_1000 | PLAYER_STATE3_8000),
              "coop::pose_limits state masks out of date");

namespace {

// Only state bits whose draw code is safe on a puppet (nothing that dereferences held actors etc.).
// The same masks are re-applied on receive (coop::SanitizePlayerState).
constexpr u32 kSyncedStateFlags1 =
    PLAYER_STATE1_8000000 /* swimming */ | PLAYER_STATE1_400000 /* shield up */ | PLAYER_STATE1_ZORA_BOOMERANG_THROWN;
constexpr u32 kSyncedStateFlags2 = PLAYER_STATE2_20000000; // not drawn
constexpr u32 kSyncedStateFlags3 = PLAYER_STATE3_1000 /* goron ball */ | PLAYER_STATE3_8000 /* zora fast swim */;

uint16_t sSeq = 0;
int16_t sLoggedScene = -2; // diagnostics: log when we start sending from a new scene

coop::Vec3s16 ToVec(const Vec3s& v) {
    return { v.x, v.y, v.z };
}

} // namespace

bool PoseCapture_InGameplay() {
    return gPlayState != nullptr && GET_PLAYER(gPlayState) != nullptr && gSaveContext.gameMode == GAMEMODE_NORMAL;
}

void PoseCapture_Tick() {
    if (!coop::client::Session_IsConnected() || !PoseCapture_InGameplay() ||
        gPlayState->transitionTrigger != TRANS_TRIGGER_OFF) {
        return;
    }
    Player* player = GET_PLAYER(gPlayState);

    coop::PlayerState s;
    s.seq = ++sSeq;
    s.sceneId = gPlayState->sceneId;
    s.roomNum = gPlayState->roomCtx.curRoom.num;
    s.form = player->transformation;
    s.entrance = gSaveContext.save.entrance;
    s.pos[0] = player->actor.world.pos.x;
    s.pos[1] = player->actor.world.pos.y;
    s.pos[2] = player->actor.world.pos.z;
    s.rot = ToVec(player->actor.shape.rot);
    s.speed = player->actor.speed;
    s.mask = player->currentMask;
    s.modelGroup = player->modelGroup;
    s.shield = (uint8_t)player->currentShield;
    s.sword = (uint8_t)GET_CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD);
    s.itemAction = player->itemAction;
    s.heldItemAction = player->heldItemAction;
    s.face = player->actor.shape.face;
    s.invincibilityTimer = player->invincibilityTimer;
    s.movementFlags = player->skelAnime.movementFlags;
    s.stateFlags1 = player->stateFlags1 & kSyncedStateFlags1;
    s.stateFlags2 = player->stateFlags2 & kSyncedStateFlags2;
    s.stateFlags3 = player->stateFlags3 & kSyncedStateFlags3;
    s.headLimbRot = ToVec(player->headLimbRot);
    s.upperLimbRot = ToVec(player->upperLimbRot);
    s.upperLimbYawSecondary = player->upperLimbYawSecondary;
    s.unk_AB8 = player->unk_AB8;
    s.unk_AAA = player->unk_AAA;
    s.unk_ABC = player->unk_ABC;
    s.unk_B86[0] = player->unk_B86[0];
    s.unk_B86[1] = player->unk_B86[1];
    s.unk_B28 = player->unk_B28;
    s.unk_B10 = player->unk_B10[0];
    s.actionVar1 = player->av1.actionVar1;
    s.unk_B8E = player->unk_B8E;
    s.unk_B62 = player->unk_B62;

    const PlayerAnimationFrame* frame = (const PlayerAnimationFrame*)player->skelAnime.jointTable;
    for (int i = 0; i < coop::kPoseJoints; i++) {
        s.joints[i] = ToVec(frame->frameTable[i]);
    }
    s.appearance = frame->appearanceInfo;

    if (s.sceneId != sLoggedScene) {
        sLoggedScene = s.sceneId;
        SPDLOG_INFO("[Coop] Sending our pose from scene {} room {}", (int)s.sceneId, (int)s.roomNum);
    }
    coop::client::NetClient::Get().SendStream(coop::EncodePlayerState(s));
}

static void RegisterPoseCapture() {
    // End of frame: animations are final and the draw has run.
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnGameStateMainFinish>(PoseCapture_Tick);
}

static RegisterShipInitFunc sPoseCaptureInit(RegisterPoseCapture);

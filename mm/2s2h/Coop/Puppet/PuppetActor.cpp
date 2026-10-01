// [COOP] En_CoopPuppet: a remote player's Link. It never runs Link's logic; every frame it copies the
// pose received from the network and draws itself with the engine's own Player_Draw.
#include "PuppetActor.h"

#include "PuppetManager.h"

#include "2s2h/Coop/Actors/HitSync.h"

#include "2s2h/Coop/Client/Session.h"
#include "2s2h/NameTag/NameTag.h"

#include "common/PlayerState.h"

extern "C" {
#include "functions.h"
#include "variables.h"
#include "z64malloc.h"
void ResourceMgr_UnregisterSkeleton(SkelAnime* skelAnime);    // BenPort.cpp
extern PlayerAgeProperties sPlayerAgeProperties[PLAYER_FORM_MAX]; // z_player.c
extern s32 D_801F59E0;                                             // z_player_lib.c: hand model index
void Player_Draw(Actor* thisx, PlayState* play);
}

namespace {

// Height of the nick above the feet, per form (PLAYER_FORM_*).
int16_t NameTagOffset(u8 form) {
    switch (form) {
        case PLAYER_FORM_FIERCE_DEITY:
            return 90;
        case PLAYER_FORM_GORON:
            return 72;
        case PLAYER_FORM_ZORA:
            return 62;
        case PLAYER_FORM_DEKU:
            return 40;
        default:
            return 52;
    }
}

Vec3s ToVec3s(const coop::Vec3s16& v) {
    return { v.x, v.y, v.z };
}

void ApplyState(EnCoopPuppet* self, const coop::PlayerState& st, PlayState* play) {
    Player* p = &self->player;

    p->actor.prevPos = p->actor.world.pos;
    p->actor.world.pos = { st.pos[0], st.pos[1], st.pos[2] };
    p->actor.shape.rot = ToVec3s(st.rot);
    p->actor.world.rot.y = st.rot.y;
    p->yaw = st.rot.y;
    p->actor.speed = st.speed;
    p->actor.shape.face = st.face;
    p->currentMask = st.mask;
    p->currentShield = (s8)st.shield;
    p->itemAction = st.itemAction;
    p->heldItemAction = st.heldItemAction;
    p->invincibilityTimer = st.invincibilityTimer;
    p->skelAnime.movementFlags = st.movementFlags;
    p->stateFlags1 = st.stateFlags1;
    p->stateFlags2 = st.stateFlags2;
    p->stateFlags3 = st.stateFlags3;
    p->headLimbRot = ToVec3s(st.headLimbRot);
    p->upperLimbRot = ToVec3s(st.upperLimbRot);
    p->upperLimbYawSecondary = st.upperLimbYawSecondary;
    p->unk_AB8 = st.unk_AB8;
    p->unk_AAA = st.unk_AAA;
    p->unk_ABC = st.unk_ABC;
    p->unk_B86[0] = st.unk_B86[0];
    p->unk_B86[1] = st.unk_B86[1];
    p->unk_B28 = st.unk_B28;
    p->unk_B10[0] = st.unk_B10;
    p->av1.actionVar1 = st.actionVar1;
    p->unk_B8E = st.unk_B8E;
    p->unk_B62 = st.unk_B62;

    PlayerAnimationFrame* frame = (PlayerAnimationFrame*)p->skelAnime.jointTable;
    for (int i = 0; i < coop::kPoseJoints; i++) {
        frame->frameTable[i] = ToVec3s(st.joints[i]);
    }
    frame->appearanceInfo = st.appearance;

    // Hand/sheath models follow the remote model group. Player_SetModels writes a global shared with
    // the local Link, so keep it untouched.
    s32 savedHandIndex = D_801F59E0;
    Player_SetModelGroup(p, (PlayerModelGroup)st.modelGroup);
    D_801F59E0 = savedHandIndex;

    func_801229FC(p); // advances the mask "loading" state so masks appear
    self->sword = st.sword;
    self->hidden = st.roomNum != play->roomCtx.curRoom.num && st.roomNum != play->roomCtx.prevRoom.num;
    self->hasState = true;

    Actor_UpdateBgCheckInfo(play, &p->actor, 0.0f, 0.0f, 0.0f, UPDBGCHECKINFO_FLAG_4); // floor for the shadow
}

} // namespace

static void EnCoopPuppet_Init(Actor* thisx, PlayState* play) {
    EnCoopPuppet* self = (EnCoopPuppet*)thisx;
    Player* p = &self->player;
    u8 form = COOP_PUPPET_GET_FORM(thisx);
    if (form >= PLAYER_FORM_MAX) {
        form = PLAYER_FORM_HUMAN;
    }
    self->playerId = COOP_PUPPET_GET_PLAYER_ID(thisx);

    p->actor.room = -1; // survives room changes like the real Link
    p->csId = CS_ID_NONE;
    p->transformation = form;
    p->ageProperties = &sPlayerAgeProperties[form];
    p->heldItemAction = PLAYER_IA_NONE;
    p->itemAction = PLAYER_IA_NONE;

    s32 savedHandIndex = D_801F59E0;
    Player_SetModelGroup(p, PLAYER_MODELGROUP_DEFAULT);
    play->playerInit(p, play, gPlayerSkeletons[form]);
    D_801F59E0 = savedHandIndex;

    // The engine only gives Fierce Deity his larger scale to ACTOR_PLAYER (func_80123140); a puppet keeps the
    // default 0.01 and looks tiny.
    Actor_SetScale(&p->actor, form == PLAYER_FORM_FIERCE_DEITY ? 0.015f : 0.01f);

    p->maskObjectSegment = ZeldaArena_Malloc(0x3800);
    p->getItemDrawIdPlusOne = GID_NONE + 1;
    // Sub-project A: puppets never block or hit anything (the shield collider is registered while drawing).
    p->shieldQuad.base.atFlags &= ~AT_ON;
    p->shieldQuad.base.acFlags &= ~AC_ON;
    p->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
    p->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;

    if (const coop::client::RemotePlayer* remote = coop::client::Session_FindPlayer(self->playerId)) {
        NameTagOptions options = {};
        options.tag = "coop";
        options.yOffset = NameTagOffset(form);
        NameTag_RegisterForActorWithOptions(thisx, remote->nick.c_str(), options);
    }
    self->initialized = true;
}

static void EnCoopPuppet_Destroy(Actor* thisx, PlayState* play) {
    EnCoopPuppet* self = (EnCoopPuppet*)thisx;
    Player* p = &self->player;
    PuppetManager_OnPuppetDestroyed(self->playerId, thisx);
    NameTag_RemoveAllForActor(thisx);
    if (!self->initialized) {
        return;
    }
    ResourceMgr_UnregisterSkeleton(&p->skelAnime);
    ResourceMgr_UnregisterSkeleton(&p->skelAnimeUpper);
    Collider_DestroyCylinder(play, &p->cylinder);
    Collider_DestroyCylinder(play, &p->shieldCylinder);
    Collider_DestroyQuad(play, &p->meleeWeaponQuads[0]);
    Collider_DestroyQuad(play, &p->meleeWeaponQuads[1]);
    Collider_DestroyQuad(play, &p->shieldQuad);
    ZeldaArena_Free(p->maskObjectSegment);
}

static void EnCoopPuppet_Draw(Actor* thisx, PlayState* play);

static void EnCoopPuppet_Update(Actor* thisx, PlayState* play) {
    EnCoopPuppet* self = (EnCoopPuppet*)thisx;
    coop::PlayerState state;
    if (PuppetManager_GetCurrent(self->playerId, state)) {
        ApplyState(self, state, play);
    }
    coop::client::HitSync_PuppetUpdate(thisx, self->playerId, play); // [COOP] C: enemies of ours can hit it
    // No draw function = no body, no shadow and no nametag (NameTag only follows drawn actors).
    bool visible = self->hasState && !self->hidden && !(self->player.stateFlags2 & PLAYER_STATE2_20000000);
    thisx->draw = visible ? EnCoopPuppet_Draw : nullptr;
}

static void EnCoopPuppet_Draw(Actor* thisx, PlayState* play) {
    EnCoopPuppet* self = (EnCoopPuppet*)thisx;
    if (self->hidden || !self->hasState) {
        return;
    }
    // The draw code reads a few globals that belong to the local Link: swap in the remote values.
    s32 savedHandIndex = D_801F59E0;
    u16 savedEquipment = gSaveContext.save.saveInfo.equips.equipment;
    u16 savedJinx = gSaveContext.jinxTimer;
    D_801F59E0 = self->player.transformation * 2;
    SET_EQUIP_VALUE(EQUIP_TYPE_SWORD, self->sword);
    gSaveContext.jinxTimer = 0;

    Player_Draw(thisx, play);

    D_801F59E0 = savedHandIndex;
    gSaveContext.save.saveInfo.equips.equipment = savedEquipment;
    gSaveContext.jinxTimer = savedJinx;
}

extern "C" ActorProfile En_CoopPuppet_Profile = {
    /**/ ACTOR_EN_COOP_PUPPET,
    /**/ ACTORCAT_NPC,
    /**/ ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnCoopPuppet),
    /**/ EnCoopPuppet_Init,
    /**/ EnCoopPuppet_Destroy,
    /**/ EnCoopPuppet_Update,
    /**/ EnCoopPuppet_Draw,
    /**/ nullptr,
};

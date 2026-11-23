// [COOP] An object slot (an index into play->objectCtx.slots) kept in an actor's own fields is this game's: the same
// object sits in another slot in another game (which room loaded it first, Epona's object, the players' objects...).
// Copied from another game it named another object, or one this game never loads. Anju waits in her update until the
// objects her fields name are loaded (EnAn_InitObjectSlots) and is not drawn meanwhile: with another game's indices she
// became ours (we talked to her) and stayed invisible for the whole cycle, still talking. Gorman does the same.
// Those bytes never travel (ActorMemory_BuildLocalMask); in a copy the ones of a fixed object are looked up here every
// frame (ObjectSlots_Refresh), so they are right the moment it becomes ours. ADD A LINE for another actor that keeps
// one (its header names it "...ObjectSlot").
#include "ActorMemory.h"

#include <cstddef>

extern "C" {
#include "functions.h"
#include "variables.h"
#include "overlays/actors/ovl_Bg_Breakwall/z_bg_breakwall.h"
#include "overlays/actors/ovl_Bg_Danpei_Movebg/z_bg_danpei_movebg.h"
#include "overlays/actors/ovl_Dm_An/z_dm_an.h"
#include "overlays/actors/ovl_Dm_Gm/z_dm_gm.h"
#include "overlays/actors/ovl_Dm_Stk/z_dm_stk.h"
#include "overlays/actors/ovl_Door_Shutter/z_door_shutter.h"
#include "overlays/actors/ovl_Door_Spiral/z_door_spiral.h"
#include "overlays/actors/ovl_En_An/z_en_an.h"
#include "overlays/actors/ovl_En_Dnk/z_en_dnk.h"
#include "overlays/actors/ovl_En_Drs/z_en_drs.h"
#include "overlays/actors/ovl_En_Gm/z_en_gm.h"
#include "overlays/actors/ovl_En_Go/z_en_go.h"
#include "overlays/actors/ovl_En_Invadepoh/z_en_invadepoh.h"
#include "overlays/actors/ovl_En_Ishi/z_en_ishi.h"
#include "overlays/actors/ovl_En_Kusa/z_en_kusa.h"
#include "overlays/actors/ovl_En_Nwc/z_en_nwc.h"
#include "overlays/actors/ovl_En_Ossan/z_en_ossan.h"
#include "overlays/actors/ovl_En_Sob1/z_en_sob1.h"
#include "overlays/actors/ovl_En_St/z_en_st.h"
#include "overlays/actors/ovl_Obj_Pzlblock/z_obj_pzlblock.h"
#include "overlays/actors/ovl_Obj_Tsubo/z_obj_tsubo.h"
}

namespace coop::client {

namespace {

constexpr int16_t kOwnInit = -1; // set by its Init from its params: every game's Init sets its own, it only must not travel

struct SlotField {
    int16_t actorId;
    uint16_t offset;
    int16_t objectId; // the object it names (kOwnInit: the one its Init chose)
};

#define FIELD(actor, type, field, object) { actor, (uint16_t)offsetof(type, field), object }

const SlotField kFields[] = {
    // Anju and Gorman look their objects up again on every room change, in their update (a copy never runs it)
    FIELD(ACTOR_EN_AN, EnAn, an2ObjectSlot, OBJECT_AN2),
    FIELD(ACTOR_EN_AN, EnAn, an3ObjectSlot, OBJECT_AN3),
    FIELD(ACTOR_EN_AN, EnAn, maskKerfayObjectSlot, OBJECT_MASK_KERFAY),
    FIELD(ACTOR_EN_AN, EnAn, an4ObjectSlot, OBJECT_AN4),
    FIELD(ACTOR_EN_AN, EnAn, msmoObjectSlot, OBJECT_MSMO),
    FIELD(ACTOR_EN_GM, EnGm, objectSlot, OBJECT_IN2),
    FIELD(ACTOR_DM_AN, DmAn, an4ObjectSlot, OBJECT_AN4),
    FIELD(ACTOR_DM_AN, DmAn, msmoObjectSlot, OBJECT_MSMO),
    FIELD(ACTOR_DM_GM, DmGm, an4ObjectSlot, OBJECT_AN4),
    FIELD(ACTOR_DM_GM, DmGm, msmoObjectSlot, OBJECT_MSMO),
    FIELD(ACTOR_DM_STK, DmStk, objectStkObjectSlot, OBJECT_STK),
    FIELD(ACTOR_DM_STK, DmStk, objectStk2ObjectSlot, OBJECT_STK2),
    FIELD(ACTOR_DM_STK, DmStk, objectStk3ObjectSlot, OBJECT_STK3),
    FIELD(ACTOR_EN_DRS, EnDrs, moonMaskObjectSlot, OBJECT_MSMO),
    FIELD(ACTOR_EN_GO, EnGo, taisouObjectSlot, OBJECT_TAISOU),
    FIELD(ACTOR_EN_GO, EnGo, hakuginDemoObjectSlot, OBJECT_HAKUGIN_DEMO),
    FIELD(ACTOR_EN_NWC, EnNwc, niwObjectSlot, OBJECT_NIW),
    FIELD(ACTOR_EN_ST, EnSt, objectSlot, GAMEPLAY_KEEP),
    // Chosen by their params in their Init
    FIELD(ACTOR_EN_DNK, EnDnk, objectSlot, kOwnInit),
    FIELD(ACTOR_EN_SOB1, EnSob1, mainObjectSlot, kOwnInit),
    FIELD(ACTOR_EN_SOB1, EnSob1, unusedObjectSlot, kOwnInit),
    FIELD(ACTOR_EN_SOB1, EnSob1, shopkeeperAnimObjectSlot, kOwnInit),
    FIELD(ACTOR_EN_OSSAN, EnOssan, objectSlot, kOwnInit),
    FIELD(ACTOR_EN_INVADEPOH, EnInvadepoh, objectSlot, kOwnInit),
    FIELD(ACTOR_DOOR_SHUTTER, DoorShutter, objectSlot, kOwnInit),
    FIELD(ACTOR_DOOR_SPIRAL, DoorSpiral, objectSlot, kOwnInit),
    FIELD(ACTOR_BG_BREAKWALL, BgBreakwall, objectSlot, kOwnInit),
    FIELD(ACTOR_BG_DANPEI_MOVEBG, BgDanpeiMovebg, objectSlot, kOwnInit),
    FIELD(ACTOR_OBJ_PZLBLOCK, ObjPzlblock, objectSlot, kOwnInit),
    // Props a Link carries travel while carried (Sync/PlayerObjects.cpp)
    FIELD(ACTOR_OBJ_TSUBO, ObjTsubo, objectSlot, kOwnInit),
    FIELD(ACTOR_EN_ISHI, EnIshi, objectSlot, kOwnInit),
    FIELD(ACTOR_EN_KUSA, EnKusa, objectSlot, kOwnInit),
};

#undef FIELD

} // namespace

void ObjectSlots_MarkLocal(TrackedActor& t) {
    for (const SlotField& f : kFields) {
        if (f.actorId == t.actor->id && f.offset < t.localMask.size()) {
            t.localMask[f.offset] = 1;
        }
    }
}

void ObjectSlots_Refresh(TrackedActor& t) {
    if (gPlayState == nullptr) {
        return;
    }
    for (const SlotField& f : kFields) {
        if (f.actorId == t.actor->id && f.objectId != kOwnInit && f.offset < t.localMask.size()) {
            *((s8*)t.actor + f.offset) = (s8)Object_GetSlot(&gPlayState->objectCtx, f.objectId);
        }
    }
}

} // namespace coop::client

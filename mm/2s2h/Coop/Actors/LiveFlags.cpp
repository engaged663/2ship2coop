// [COOP] See LiveFlags.h. Editable table: which actor shows which flag, and how to read it from the actor.
#include "LiveFlags.h"

#include "2s2h/Coop/Sync/Sync.h"

#include <spdlog/spdlog.h>

extern "C" {
#include "z64.h"
#include "variables.h"
#include "overlays/actors/ovl_En_Elforg/z_en_elforg.h"
#include "overlays/actors/ovl_En_Si/z_en_si.h"
}

namespace coop::client {

namespace {

// The flag an actor stands for, or -1 when it has none of that type.
using FlagOf = int (*)(Actor* actor);

int ItemCollectible(Actor* a) {
    return ((EnItem00*)a)->collectibleFlag;
}

int HeartContainer(Actor*) {
    return 0x1F; // Item_B_Heart always uses collectible 0x1F
}

int FairyCollectible(Actor* a) {
    return STRAY_FAIRY_TYPE(a) == STRAY_FAIRY_TYPE_COLLECTIBLE ? STRAY_FAIRY_GET_FLAG(a) : -1;
}

int FairyChest(Actor* a) {
    return STRAY_FAIRY_TYPE(a) == STRAY_FAIRY_TYPE_CHEST ? STRAY_FAIRY_GET_FLAG(a) : -1;
}

// The stray fairies whose flag is a switch (EnElforg_Init: every type without a case of its own).
int FairySwitch(Actor* a) {
    switch (STRAY_FAIRY_TYPE(a)) {
        case STRAY_FAIRY_TYPE_CLOCK_TOWN:
        case STRAY_FAIRY_TYPE_COLLECTIBLE:
        case STRAY_FAIRY_TYPE_FAIRY_FOUNTAIN:
        case STRAY_FAIRY_TYPE_BUBBLE:
        case STRAY_FAIRY_TYPE_CHEST:
        case STRAY_FAIRY_TYPE_RETURNING_TO_FOUNTAIN:
            return -1;
        default:
            return STRAY_FAIRY_GET_FLAG(a);
    }
}

int SkulltulaToken(Actor* a) {
    return ENSI_GET_CHEST_FLAG(a);
}

struct LiveFlagActor {
    LiveFlagType type;
    int16_t actorId;
    FlagOf flagOf;
};

const LiveFlagActor kLiveFlagActors[] = {
    { LiveFlagType::Collectible, ACTOR_EN_ITEM00, ItemCollectible },
    { LiveFlagType::Collectible, ACTOR_ITEM_B_HEART, HeartContainer },
    { LiveFlagType::Collectible, ACTOR_EN_ELFORG, FairyCollectible },
    { LiveFlagType::Chest, ACTOR_EN_SI, SkulltulaToken },
    { LiveFlagType::Chest, ACTOR_EN_ELFORG, FairyChest },
    { LiveFlagType::Switch, ACTOR_EN_ELFORG, FairySwitch },
};

} // namespace

void LiveFlags_OnRemoteFlag(LiveFlagType type, int flag) {
    if (gPlayState == nullptr) {
        return;
    }
    Player* player = (Player*)gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first;
    for (const LiveFlagActor& entry : kLiveFlagActors) {
        if (entry.type != type) {
            continue;
        }
        for (int cat = 0; cat < ACTORCAT_MAX; cat++) {
            for (Actor* a = gPlayState->actorCtx.actorLists[cat].first; a != nullptr; a = a->next) {
                // A shared item is removed one by one by Sync/SharedDrops.cpp ("item_gone"): its flag may be shared by
                // others still lying there (all the rupees of a formation, the three hearts of a pot)
                if (a->id != entry.actorId || a->update == nullptr || entry.flagOf(a) != flag ||
                    (a->id == ACTOR_EN_ITEM00 && SharedDrops_Tracks(a)) || (player != nullptr && player->heldActor == a)) {
                    continue;
                }
                SPDLOG_INFO("[Coop] Someone else took actor {:#x} (flag {} of type {}): removed", (uint16_t)a->id, flag,
                            (int)type);
                Actor_Kill(a);
            }
        }
    }
}

} // namespace coop::client

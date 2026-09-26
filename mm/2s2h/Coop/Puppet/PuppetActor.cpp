#include "PuppetActor.h"

extern "C" {
#include "functions.h"
#include "variables.h"
}

// Placeholder bodies; the real implementation replaces them (see plan task "Puppet actor").
static void EnCoopPuppet_Init(Actor* thisx, PlayState* play) {
    Actor_Kill(thisx);
}

static void EnCoopPuppet_Destroy(Actor* thisx, PlayState* play) {
}

static void EnCoopPuppet_Update(Actor* thisx, PlayState* play) {
}

static void EnCoopPuppet_Draw(Actor* thisx, PlayState* play) {
}

extern "C" ActorProfile En_CoopPuppet_Profile = {
    /**/ ACTOR_EN_COOP_PUPPET,
    /**/ ACTORCAT_NPC,
    /**/ 0,
    /**/ GAMEPLAY_KEEP,
    /**/ sizeof(EnCoopPuppet),
    /**/ EnCoopPuppet_Init,
    /**/ EnCoopPuppet_Destroy,
    /**/ EnCoopPuppet_Update,
    /**/ EnCoopPuppet_Draw,
    /**/ nullptr,
};

// [COOP] See ReplicationRules.h. Lists built from the decomp (scripts that looked at every overlay's category and
// Actor_Spawn calls); the reason of each entry is next to it.
#include "ReplicationRules.h"

#include "2s2h/Coop/Activities/Activities.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <initializer_list>

namespace coop::client {

namespace {

bool In(int16_t id, std::initializer_list<int16_t> list) {
    return std::find(list.begin(), list.end(), id) != list.end();
}

// Never replicated, whatever their category.
bool NeverReplicated(int16_t id) {
    return In(id, {
        ACTOR_EN_COOP_PUPPET, // the other players' Links
        ACTOR_PLAYER,
        ACTOR_EN_ITEM00,  // drops: each game drops its own (DropSync.cpp)
        ACTOR_ITEM_B_HEART, // a heart container (category BOSS): each game has its own to pick up (Echo below)
        ACTOR_EN_WATER_EFFECT, // splashes (category BOSS)
        ACTOR_EFF_DUST,   // dust (category NPC), made by the player's own attacks too
        ACTOR_EN_SDA,     // the player's dynamic shadow (category BOSS): each game draws its own Link's
        ACTOR_ITEM_INBOX, ACTOR_OBJ_DORA, ACTOR_OBJ_KENDO_KANBAN, // fixtures filed as NPC
        ACTOR_EN_JS, // the Moon's children: the masks given to them are each player's own (masksGivenOnMoon)
    });
}

// Cutscene actors: a cutscene plays in the game that started it. They are ours, except while we watch another
// game's shared cutscene: then that game's copy drives ours (Replication::Cinema, Features/Cinema.cpp).
bool IsCutsceneActor(int16_t id) {
    return In(id, {
        ACTOR_DEMO_EFFECT, ACTOR_DEMO_GETITEM, ACTOR_DEMO_KANKYO, ACTOR_DEMO_MOONEND, ACTOR_DEMO_SHD, ACTOR_DEMO_SYOTEN,
        ACTOR_DEMO_TRE_LGT, ACTOR_DM_AH, ACTOR_DM_AL, ACTOR_DM_AN, ACTOR_DM_BAL, ACTOR_DM_CHAR00, ACTOR_DM_CHAR01,
        ACTOR_DM_CHAR02, ACTOR_DM_CHAR03, ACTOR_DM_CHAR04, ACTOR_DM_CHAR05, ACTOR_DM_CHAR06, ACTOR_DM_CHAR07,
        ACTOR_DM_CHAR08, ACTOR_DM_CHAR09, ACTOR_DM_GM, ACTOR_DM_HINA, ACTOR_DM_NB, ACTOR_DM_OPSTAGE, ACTOR_DM_RAVINE,
        ACTOR_DM_SA, ACTOR_DM_STATUE, ACTOR_DM_STK, ACTOR_DM_TAG, ACTOR_DM_TSG, ACTOR_DM_ZL,
    });
}

// gCoop.Group.CinemaActors = 0: as before the limits fix (each game's own, never followed).
Replication CutsceneRule() {
    return Rules_CinemaActorsOn() ? Replication::Cinema : Replication::Local;
}

// Actors of other categories that create enemies on their own: they must run in one game only, or every game would
// create its own enemies. (The ones that create them when the player breaks or cuts them stay local.)
bool IsSpawner(int16_t id) {
    return In(id, {
        ACTOR_EN_ENCOUNT1, // field encounters: dragonflies, Skulltulas, Wallmasters
        ACTOR_EN_ENCOUNT4, // Stalchildren
        ACTOR_BG_SINKAI_KABE, // Deep Python wall (Pinnacle Rock)
    });
}

// Created by a replicated actor, but each player needs their own: to pick it up, enter it or open it.
bool IsEcho(int16_t id) {
    return In(id, {
        ACTOR_DOOR_WARP1,  // the warp after a boss
        ACTOR_ITEM_B_HEART, // heart container
        ACTOR_EN_SI,       // Gold Skulltula token
        ACTOR_EN_ELFORG,   // stray fairy
        ACTOR_EN_BOX,      // treasure chest
    });
}

bool TrackedCategory(uint8_t category) {
    return category == ACTORCAT_NPC || category == ACTORCAT_ENEMY || category == ACTORCAT_BOSS;
}

// Actors of the room's list filed as props or scenery that attack, chase or move by themselves: they must be the
// same for everyone. Not every prop (a pot or a crate is scenery: PropSync.cpp removes it for everyone when broken),
// so they are named one by one.
bool IsPropEnemy(int16_t id) {
    return In(id, {
        ACTOR_EN_NIW,          // cucco: can be picked up and carried (and attacks in flocks)
        ACTOR_EN_WF,           // Wolfos / White Wolfos (filed as a prop)
        ACTOR_EN_INVADEPOH,    // the aliens of Romani Ranch (and their invasion)
        ACTOR_EN_GOROIWA,      // rolling boulders
        ACTOR_EN_HONOTRAP,     // fire-shooting eye statues
        ACTOR_EN_TUBO_TRAP,    // flying pot traps
        ACTOR_EN_RAF,          // carnivorous lily pads
        ACTOR_OBJ_TOGE,        // blade traps
        ACTOR_OBJ_SPINYROLL,   // spiked logs
        ACTOR_OBJ_VSPINYROLL,  // vertical spike rollers
        ACTOR_BG_ICICLE,       // falling icicles
        ACTOR_EN_KUSA2,        // Keaton's grass: it moves around when hit and brings Keaton (one game runs it)
    });
}

} // namespace

Replication Rules_ListActor(int16_t actorId, uint8_t category) {
    if (NeverReplicated(actorId)) {
        return Replication::Local;
    }
    if (IsCutsceneActor(actorId)) {
        return CutsceneRule();
    }
    return (TrackedCategory(category) || IsSpawner(actorId) || IsPropEnemy(actorId) || Activity_IsPropId(actorId))
               ? Replication::Replicated
               : Replication::Local;
}

Replication Rules_RuntimeChild(int16_t actorId, uint8_t category) {
    if (IsEcho(actorId)) {
        return Replication::Echo;
    }
    if (actorId == ACTOR_EN_HORSE) {
        // Epona stays local to each game (her Init needs her own object files loaded; a copy made at run time
        // crashes in them). Ride.cpp glues the passenger's own horse to the driver's instead.
        return Replication::Local;
    }
    if (NeverReplicated(actorId) || category == ACTORCAT_PLAYER) {
        return Replication::Local;
    }
    if (IsCutsceneActor(actorId)) {
        return CutsceneRule();
    }
    // Projectiles, sparks, rocks, platforms, other enemies: the copies follow the one that made them.
    return Replication::Replicated;
}

bool Rules_Leasable(int16_t actorId, uint8_t category) {
    return category == ACTORCAT_NPC && !NeverReplicated(actorId) && !IsCutsceneActor(actorId);
}

bool Rules_IsCutsceneActor(int16_t actorId) {
    return IsCutsceneActor(actorId);
}

bool Rules_CinemaActorsOn() {
    return CVarGetInteger("gCoop.Group.CinemaActors", 1) != 0;
}

} // namespace coop::client

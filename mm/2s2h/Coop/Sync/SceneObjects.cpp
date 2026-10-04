// [COOP] Sincronización total S5 (spec §6): the scene's machinery that is the same for everyone. kSceneObjects lists the
// actors that only move (and maybe react to who stands on them): they are replicated like the enemies, and who
// touches one runs it (Touch: standing on it, or a bomb/statue/pot of ours on it; Near: within reach, the push
// blocks), so its reaction, its cutscene and its carry happen in that player's game. The others (doors, what talks,
// gives items, warps or moves the player) stay each game's own: the scan of 2026-10-04 is in the spec.
// gCoop.Sync.Shared / gCoop.Sync.Local: more ids or names (GameIds.inc), comma separated, without recompiling.
#include "Sync.h"

#include "2s2h/Coop/Actors/ActorRegistry.h"
#include "2s2h/Coop/Actors/CoopEngine.h"
#include "2s2h/Coop/Client/Session.h"

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include "functions.h"
#include "variables.h"
}

namespace coop::client {

namespace {

struct SceneObject {
    int16_t id;
    TouchPolicy policy;
};

// ADD A LINE (read the actor first: no writes to the player, no talking, no items, no warps; a cutscene is fine: it
// plays in the game of who touches it).
const SceneObject kSceneObjects[] = {
    { ACTOR_BG_DANPEI_MOVEBG, TouchPolicy::Touch },  // moving platforms under the graveyard
    { ACTOR_BG_DBLUE_ELEVATOR, TouchPolicy::Touch }, // Great Bay Temple's lifts
    { ACTOR_BG_F40_FLIFT, TouchPolicy::Touch },      // Stone Tower's floating lift
    { ACTOR_BG_F40_SWLIFT, TouchPolicy::Touch },     // Stone Tower's switch lift
    { ACTOR_BG_LOTUS, TouchPolicy::Touch },          // Woodfall's lily pads (they sink under who stands on them)
    { ACTOR_OBJ_DANPEILIFT, TouchPolicy::Touch },    // lifts that rise when stood on
    { ACTOR_OBJ_DRIFTICE, TouchPolicy::Touch },      // drifting ice
    { ACTOR_OBJ_ROTLIFT, TouchPolicy::Touch },       // rotating lifts (their Deku flowers stay each game's)
    { ACTOR_OBJ_Y2LIFT, TouchPolicy::Touch },        // Pirates' fortress lifts
    { ACTOR_OBJ_LIFT, TouchPolicy::Touch },          // platforms that fall when stood on
    { ACTOR_OBJ_RAILLIFT, TouchPolicy::Touch },      // platforms on rails (Deku flowers on top: each game's)
    { ACTOR_OBJ_CHIKUWA, TouchPolicy::Touch },       // falling blocks
    { ACTOR_OBJ_BOAT, TouchPolicy::Touch },          // the pirates' boats
    { ACTOR_OBJ_OCARINALIFT, TouchPolicy::Touch },   // lifts moved by a song
    { ACTOR_OBJ_SWITCH, TouchPolicy::Touch },        // floor, crystal and eye switches (hits travel with "hit")
    { ACTOR_BG_F40_SWITCH, TouchPolicy::Touch },     // Stone Tower's switches
    { ACTOR_BG_HAKUGIN_SWITCH, TouchPolicy::Touch }, // Snowhead's Goron switches
    { ACTOR_OBJ_LIGHTSWITCH, TouchPolicy::None },    // sun switches (light arrows hit it from afar)
    { ACTOR_OBJ_OSHIHIKI, TouchPolicy::Near },       // push blocks
    { ACTOR_OBJ_PZLBLOCK, TouchPolicy::Near },       // puzzle blocks
    { ACTOR_OBJ_ARMOS, TouchPolicy::Near },          // Armos statues that are pushed
    { ACTOR_BG_KIN2_SHELF, TouchPolicy::Near },      // the Spider House's shelves
    { ACTOR_BG_DBLUE_MOVEBG, TouchPolicy::Near },    // Great Bay Temple's pushed pieces
    { ACTOR_BG_IKANA_BLOCK, TouchPolicy::Near },     // Ikana's push blocks
};

constexpr float kNearDist = 150.f;
constexpr int64_t kTouchGraceMs = 1000; // after stepping off it stays ours this long (spec §6.2: "and 1 s more")

struct NamedActor {
    const char* name;
    int16_t id;
};

#define COOP_ITEM(name, id)
#define COOP_ACTOR(name, id, title) { #name, (int16_t)(id) },
#define COOP_SCENE(key, decomp, scene, entrance, title)
const NamedActor kActorNames[] = {
#include "common/GameIds.inc"
};
#undef COOP_ITEM
#undef COOP_ACTOR
#undef COOP_SCENE

std::unordered_map<const Actor*, int64_t> sTouchedUntil;

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string Upper(std::string s) {
    for (char& c : s) {
        c = (char)std::toupper((unsigned char)c);
    }
    return s;
}

// "OBJ_LIFT, 0x1C3, 451, actor_obj_raillift" -> ids
std::vector<int16_t> ParseList(const std::string& text) {
    std::vector<int16_t> out;
    std::string token;
    auto flush = [&]() {
        if (token.empty()) {
            return;
        }
        std::string t = Upper(token);
        token.clear();
        if (t.rfind("ACTOR_", 0) == 0) {
            t = t.substr(6);
        }
        char* end = nullptr;
        long v = std::strtol(t.c_str(), &end, 0);
        if (end != t.c_str() && *end == '\0') {
            if (v >= 0 && v < ACTOR_ID_MAX) {
                out.push_back((int16_t)v);
            }
            return;
        }
        for (const NamedActor& n : kActorNames) {
            if (t == n.name) {
                out.push_back(n.id);
                return;
            }
        }
    };
    for (char c : text) {
        if (c == ',' || c == ';' || std::isspace((unsigned char)c)) {
            flush();
        } else {
            token += c;
        }
    }
    flush();
    return out;
}

// A CVar list, parsed again only when its text changes.
struct CVarList {
    const char* cvar;
    std::string text = "\x01";
    std::vector<int16_t> ids;
    bool Has(int16_t id) {
        const char* now = CVarGetString(cvar, "");
        if (text != now) {
            text = now;
            ids = ParseList(text);
        }
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }
};
CVarList sShared{ "gCoop.Sync.Shared" };
CVarList sLocal{ "gCoop.Sync.Local" };

const SceneObject* Listed(int16_t id) {
    for (const SceneObject& o : kSceneObjects) {
        if (o.id == id) {
            return &o;
        }
    }
    return nullptr;
}

void Touch(const Actor* a) {
    if (a != nullptr) {
        sTouchedUntil[a] = NowMs() + kTouchGraceMs;
    }
}

// After our Link updates: what it stands on.
void Tick(Actor* linkActor) {
    PlayState* play = gPlayState;
    Player* link = (Player*)linkActor;
    if (play == nullptr || link == nullptr || !Sync_On(SyncPart::SceneObjects)) {
        return;
    }
    if ((link->actor.bgCheckFlags & BGCHECKFLAG_GROUND) && link->actor.floorBgId != BGCHECK_SCENE) {
        Touch((const Actor*)DynaPoly_GetActor(&play->colCtx, link->actor.floorBgId));
    }
    int64_t now = NowMs();
    for (auto it = sTouchedUntil.begin(); it != sTouchedUntil.end();) {
        it = it->second < now - 5000 ? sTouchedUntil.erase(it) : std::next(it);
    }
}

void RegisterSceneObjects() {
    COND_ID_HOOK(OnActorUpdate, ACTOR_PLAYER, true, Tick);
    COND_HOOK(OnActorDestroy, true, [](Actor* actor) { sTouchedUntil.erase(actor); });
    COND_HOOK(OnPlayDestroy, true, []() { sTouchedUntil.clear(); });
}

} // namespace

bool SceneObjects_ForcedLocal(int16_t actorId) {
    return sLocal.Has(actorId);
}

bool SceneObjects_Shared(int16_t actorId) {
    if (!Sync_On(SyncPart::SceneObjects) || SceneObjects_ForcedLocal(actorId)) {
        return false;
    }
    return Listed(actorId) != nullptr || sShared.Has(actorId);
}

TouchPolicy SceneObjects_Policy(int16_t actorId) {
    const SceneObject* o = Listed(actorId);
    if (o != nullptr) {
        return o->policy;
    }
    return sShared.Has(actorId) ? TouchPolicy::Touch : TouchPolicy::None;
}

bool SceneObjects_Touched(const Actor* actor, float distToLink) {
    TouchPolicy policy = SceneObjects_Policy(actor->id);
    if (policy == TouchPolicy::None) {
        return false;
    }
    if (policy == TouchPolicy::Near && distToLink < kNearDist) {
        return true;
    }
    auto it = sTouchedUntil.find(actor);
    return it != sTouchedUntil.end() && it->second >= NowMs();
}

const char* SceneObjects_ActorName(int16_t actorId) {
    for (const NamedActor& n : kActorNames) {
        if (n.id == actorId) {
            return n.name;
        }
    }
    return "?";
}

static RegisterShipInitFunc sSceneObjectsInit(RegisterSceneObjects);

} // namespace coop::client

using namespace coop::client;

// Who stands on what this frame: our Link, or one of our own objects (a bomb, a statue, a pot we carried).
extern "C" void Coop_OnCarried(DynaPolyActor* dyna, Actor* carried) {
    if (dyna == nullptr || carried == nullptr || gPlayState == nullptr) {
        return;
    }
    if (carried == gPlayState->actorCtx.actorLists[ACTORCAT_PLAYER].first) {
        Touch(&dyna->actor);
        return;
    }
    TrackedActor* t = ActorRegistry_Get(carried);
    if (t != nullptr && t->owner == Session_LocalId()) {
        Touch(&dyna->actor);
    }
}

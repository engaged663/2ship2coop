// [COOP] The names the server's mods use for the game's things (coop/common/GameIds.inc, made by
// coop/tools/gen_game_ids.py) must be the game's own: a wrong line stops the build here, with its name.
extern "C" {
#include "z64.h"
}

#define COOP_ITEM(name, id) static_assert(ITEM_##name == (id), "GameIds.inc: item " #name);
#define COOP_ACTOR(name, id, title) static_assert(ACTOR_##name == (id), "GameIds.inc: actor " #name);
#define COOP_SCENE(key, decomp, scene, entrance, title) \
    static_assert(SCENE_##decomp == (scene) && ENTR_SCENE_##key == (entrance), "GameIds.inc: scene " #key);
#include "common/GameIds.inc"
#undef COOP_ITEM
#undef COOP_ACTOR
#undef COOP_SCENE

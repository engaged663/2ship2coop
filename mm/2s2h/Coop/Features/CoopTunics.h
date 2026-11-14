#ifndef COOP_TUNICS_H
#define COOP_TUNICS_H

// [COOP] Each Link's tunic in their player's color (CoopTunics.cpp, coop/common/TunicSplit.h): the other players' Links
// and, unless turned off, our own. Human form only (the other forms have the green painted in their textures). Off
// switches: gCoop.Tunic.Players (the others), gCoop.Tunic.Mine (ours), both on by default; only in a co-op session.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"

// Player_DrawImpl, before drawing the skeleton of `actor` (our Link or a co-op puppet) in `form`: the limb callback to
// draw it with. `inner` itself, or a wrapper that calls `inner` and then puts that Link's color on its tunic pieces.
OverrideLimbDrawFlex Coop_TunicWrapOverride(PlayState* play, Actor* actor, s32 form, OverrideLimbDrawFlex inner);
// Player_DrawImpl, after the skeleton: the wrapper is done with this Link.
void Coop_TunicDrawDone(void);

#ifdef __cplusplus
}

namespace coop::client {
bool CoopTunics_PlayersOn(); // the others' Links in their color
bool CoopTunics_MineOn();    // our own Link in ours
void CoopTunics_DrawMenu();  // the two switches (the "My color and the minimap" section)
} // namespace coop::client
#endif

#endif // COOP_TUNICS_H

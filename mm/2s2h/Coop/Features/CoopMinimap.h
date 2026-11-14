#ifndef COOP_MINIMAP_H
#define COOP_MINIMAP_H

// [COOP] The other players on the minimap (CoopMinimap.cpp), for z_map_disp.c: an arrow of each player's color
// (PlayerColors.h) where their Link is, pointing where it looks. Menu: gCoop.Minimap.Players / Minimap.OwnColor.

#ifdef __cplusplus
extern "C" {
#endif
#include "z64.h"

// MapDisp_Minimap_DrawActors, before the actor lists: draws every other player's puppet in our scene (and room) with
// MapDisp_Minimap_DrawActorIcon, so their arrows stay under ours.
void Coop_MinimapDrawPlayers(PlayState* play);
// MapDisp_Minimap_DrawActorIcon: 1 = draw this actor as an arrow of *color pointing at *yaw (a puppet of that pass).
// For our own Link it may only change *color (the menu's "my arrow in my color") and returns 0.
s32 Coop_MinimapArrow(Actor* actor, Color_RGB8* color, s16* yaw);

// z_map_disp.c: a point of the current room's minimap in its texture's own coordinates (u, v), and back. The same
// texture (mapId) means the same picture: Clock Town's five scenes share map 0x105. 0 = this room has no minimap
// (or, MapToWorld, shows another one).
s32 MapDisp_Coop_WorldToMap(PlayState* play, f32 x, f32 z, s32* mapId, f32* u, f32* v);
s32 MapDisp_Coop_MapToWorld(PlayState* play, s32 mapId, f32 u, f32 v, f32* x, f32* z);

#ifdef __cplusplus
}
#endif

#endif // COOP_MINIMAP_H

#pragma once
// [COOP] Players' colors in the game (common/PlayerColor.h): each player's Link's tunic (CoopTunics.cpp), their nick
// above their Link, their arrow on the minimap (CoopMinimap.cpp), their nick in the chat (Chat/ChatWindow.cpp) and in
// the player list (Menu/CoopMenu.cpp).
// Ours comes from the menu's CVars (gCoop.Color.*): it travels in "hello" and, when it changes, in a "color" event
// (Session_SetLocalColor), at most 4 times a second. Who never picked one shows their id's (DefaultPlayerColor).
//
// CVars: Color.Custom (0 = automatic), Color.Main / Color.Second (0xRRGGBB), Color.Fx (ColorFx), Color.Speed
// (ColorSpeed), Color.NameTags, Color.Chat, Minimap.Players (1 by default), Minimap.OwnColor (0 by default).
#include "common/PlayerColor.h"

#include <cstdint>
#include <string>

namespace coop::client {

int64_t PlayerColors_SettingsPacked();               // ours as the menu has it (kNoColor = automatic)
coop::PlayerColor PlayerColors_Of(uint8_t playerId);  // anyone's, ours included (Session_LocalId())
coop::Rgb PlayerColors_Shown(uint8_t playerId);       // with its effect at this moment
coop::Rgb PlayerColors_ShownOf(const coop::PlayerColor& color); // the same for a color kept aside (a chat line's)
uint8_t PlayerColors_IdOfNick(const std::string& nick);         // ours or another player's id; 0 = nobody
bool PlayerColors_ChatOn();
bool PlayerColors_MinimapOn();      // the other players' arrows
bool PlayerColors_MinimapOwnColor(); // our own arrow in our color

void PlayerColors_DrawMenu(); // the "My color and the minimap" section of the Co-op menu

} // namespace coop::client

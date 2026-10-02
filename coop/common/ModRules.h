#pragma once
// Rules of the mod system that the server and the game must agree on.
#include "Events.h"

#include <string>

namespace coop::mods {

// A game setting (a 2 Ship CVar) a server may force on the games playing in its world: gameplay options
// (gEnhancements., gCheats., gModes., gFixes.), never a player's own accessibility, camera, graphics, saving, asset
// or control options. The game also refuses the ones the co-op itself forces (WorldSession.cpp).
bool SettingAllowed(const std::string& name);
// What it may be set to: a finite number between -1e6 and 1e6 (whole numbers are integer options).
bool SettingValueAllowed(const json& value);
// An item a server may give or take (game.giveItem, game.takeItem; the game checks it again before its Item_Give):
// every id up to kLastModItem but the few the game's Item_Give has no case for.
bool ItemGivable(int id);

} // namespace coop::mods

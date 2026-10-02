#pragma once
// [COOP] The game's side of the server's mods (coop/docs/mods: Lua scripts and DLL plugins run on the server). Map:
//   GameOps.cpp       "mod": the orders of the mods for this game (one line per order in kOps)
//   ModSettings.cpp   "mod_cfg": the 2 Ship options the server forces while playing in its world (given back after)
//   GameEvents.cpp    "gev" and "stat": what happens in this game (items, deaths, bosses, kills, health...)
//   GameIdsCheck.cpp  the names the mods use for items, actors and scenes are the game's own (static_assert)
// Everything works only while playing in the server's world. Off switch: gCoop.Mods = 0 (F1 -> Co-op).

namespace coop::client {

bool Mods_Enabled();    // gCoop.Mods (1 by default) and not a headless host
bool Mods_GivingItem(); // an order is giving an item now: it is not reported back as "item"

} // namespace coop::client

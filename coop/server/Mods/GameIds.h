#pragma once
// The game's things by name, for the mods: items, actors and scenes (common/GameIds.inc, generated from the game's
// headers by coop/tools/gen_game_ids.py). Names are found in any case, with or without their prefix ("MASK_BUNNY",
// "item_mask_bunny"); a scene also by the decomp's own name ("CLOCKTOWER" is SOUTH_CLOCK_TOWN).
#include "common/Events.h"

#include <string>

namespace coop::ids {

enum class Kind { Item, Actor, Scene };

bool ParseKind(const std::string& text, Kind& out); // "item", "actor", "scene"
// The id of a name, or the number itself when the game has that id; -1 when it does not.
int Find(Kind kind, const json& nameOrId);
std::string NameOf(Kind kind, int id);  // "" when unknown
std::string TitleOf(Kind kind, int id); // "Dodongo", "South Clock Town"; "" for items and unknown ids
int EntranceSceneOf(int sceneId);       // the index of its entrance table (entrance >> 9); -1: not a scene
json Table(Kind kind);                  // {name: id}

} // namespace coop::ids

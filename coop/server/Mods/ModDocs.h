#pragma once
// The reference of the mod API, generated from the code so it never falls behind: every function (the COOP_MOD_API
// lines and the ones each engine gives, kCallbackApis in ModDocs.cpp), every event (ModEvents.inc) and the game's
// names (common/GameIds.inc). 2ship-coop-server --mod-docs coop/docs/mods writes API.md and IDS.md there; a test
// checks that the copies in the repository are the current ones.
#include <string>

namespace coop::server {

std::string ModApiMarkdown(); // the reference: every function (by namespace) and every event
std::string ModIdsMarkdown(); // items, actors and scenes with their ids
bool WriteModDocs(const std::string& dir, std::string* err); // API.md and IDS.md; err: the file or the OS's reason

} // namespace coop::server

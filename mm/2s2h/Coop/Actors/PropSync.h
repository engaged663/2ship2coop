#pragma once
// [COOP] Shared props (PropSync.cpp): pots, grass, crates, barrels, rocks and invisible rupees are broken, cut or
// picked up for everyone in the scene. What they drop is a shared item (Sync/SharedDrops.cpp).
#include <cstdint>

struct Actor;

namespace coop::client {

// One of kSharedProps: PropSync.cpp removes it for everyone (Sync/FlagReload.cpp never creates it again).
bool PropSync_IsShared(int16_t actorId);

} // namespace coop::client

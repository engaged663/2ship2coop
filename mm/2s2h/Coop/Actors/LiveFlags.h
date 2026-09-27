#pragma once
// Objects that another player picked up disappear here at once (spec §9): heart pieces, collectibles, Skulltula
// tokens and stray fairies only look at their flag when they are created, so a flag that arrives later would leave
// them in the scene until it reloads.
#include <cstdint>

namespace coop::client {

enum class LiveFlagType : uint8_t { Collectible, Chest, Switch };

// FieldTable.cpp: a remote change turned this flag of the loaded scene on.
void LiveFlags_OnRemoteFlag(LiveFlagType type, int flag);

} // namespace coop::client

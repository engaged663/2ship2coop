#pragma once
// [COOP] Dialogues seen together (spec §6; TalkSync.cpp).
#include <cstdint>

namespace coop::client {

bool TalkSync_Mirroring(); // our text box shows another player's dialogue (Cinema.cpp lets us watch with it open)
uint16_t TalkSync_MirroredTextId(); // the text our mirrored box shows (0: none)

} // namespace coop::client

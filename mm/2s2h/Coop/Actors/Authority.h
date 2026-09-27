#pragma once
// Who simulates the shared enemies of each room of our scene, as the server says ("auth", sub-project C).
#include <cstdint>

namespace coop::client {

// 0 = nobody known yet (we are alone, the table has not arrived, or we are not in the server's world).
uint8_t Authority_Owner(int8_t room);
bool Authority_IsMine(int8_t room);
bool Authority_IsRemote(int8_t room); // owned by another player: its enemies are replicas here
// True when the last table is for the loaded scene.
bool Authority_Known();

} // namespace coop::client

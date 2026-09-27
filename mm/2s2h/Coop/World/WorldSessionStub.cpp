// TEMPORARY (B task 6 only): lets the game link before WorldSession.cpp exists. Task 7 deletes this file.
#include "WorldSession.h"

namespace coop::client {

WorldState WorldSession_State() {
    return WorldState::Outside;
}

int WorldSession_Cycle() {
    return 0;
}

} // namespace coop::client

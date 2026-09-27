#pragma once
// Who simulates the shared enemies of each (scene, room) (sub-project C): pure rules, used by ActorHandlers.cpp.
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace coop::server {

struct AuthMember {
    uint8_t id = 0;
    int16_t scene = -1;
    int8_t room = -1;
    bool busy = false;    // paused, reading a text or in a cutscene: its enemies would freeze for everyone
    int64_t sinceMs = 0;  // when it arrived in that room
};

using RoomKey = std::pair<int16_t, int8_t>;

// The owner of every (scene, room) with members: the one there longest that is not busy; if all are busy, the one
// there longest (the enemies wait for it). Negative scenes and rooms are ignored. Ties: the lower id.
std::map<RoomKey, uint8_t> ComputeAuthority(const std::vector<AuthMember>& members);

} // namespace coop::server

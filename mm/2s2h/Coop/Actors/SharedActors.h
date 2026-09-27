#pragma once
// The enemies that the players of a room share (sub-project C, spec §3.3). Every other actor stays local to each
// game, as in sub-project B. To add an enemy: check it only hurts Link through colliders (no grabbing, no direct
// damage, no cutscene), then add a line to kSharedActors in SharedActors.cpp (and its draw extras if it has any).
#include <cstdint>
#include <vector>

struct Actor;

namespace coop::client {

struct SharedActorDef {
    int16_t actorId;
    // Per-enemy draw state the owner sends and the replicas apply (damage effects, alpha...). May be null.
    void (*writeExtras)(Actor* actor, std::vector<uint8_t>& out);
    void (*readExtras)(Actor* actor, const std::vector<uint8_t>& in);
};

// nullptr: that actor is not shared.
const SharedActorDef* SharedActors_Find(int16_t actorId);

} // namespace coop::client

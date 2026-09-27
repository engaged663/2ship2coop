#pragma once
// First byte of every packet on kChannelStream: which binary stream it is.
#include <cstdint>

namespace coop {

constexpr uint8_t kStreamActors = 2; // ActorImage.h (sub-project D3; ActorState.h was C1's format)

} // namespace coop

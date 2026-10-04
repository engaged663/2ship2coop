#pragma once
// First byte of every packet on kChannelStream (and on kChannelFiles): which binary stream it is.
#include <cstdint>

namespace coop {

constexpr uint8_t kStreamActors = 2; // ActorImage.h (sub-project D3; ActorState.h was C1's format)
constexpr uint8_t kStreamEponaState = 3; // EponaState.h: owner-authoritative horse list
constexpr uint8_t kStreamEffects = 4; // EffectImage.h: particles echoed to the other games of the scene
constexpr uint8_t kStreamO2r = 5; // common/O2r.h: a chunk of a game mod (.o2r), on kChannelFiles

} // namespace coop

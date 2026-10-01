#pragma once
// [COOP] Effects echo (spec 2026-09-30-coop-grupos-limites §8): the particles (EffectSs) a game creates for its Link,
// for the actors it simulates or for the cutscene it shows are created in the other games of the scene too. Binary
// stream kStreamEffects on kChannelStream:
//   [type u8 = kStreamEffects][playerId u8 (stamped by the server)][scene s16][flags u8: 1 = cutscene][count u8]
//   then per effect: [effect u8][priority u8][slotCount u8] + slots (ActorImage's slot encoding).
// The init data of an effect travels as 8-byte slots classified like an actor's memory (pointers translated).
#include "ActorImage.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

namespace effect_limits {
constexpr int kEffects = 16;       // effects in one packet
constexpr int kSlots = 16;         // 8-byte slots of one effect's init data (128 bytes: the biggest is far below)
constexpr int kEffectTypes = 0x27; // EFFECT_SS_TYPE_MAX of the game (EffectEcho.cpp checks it)
constexpr size_t kPacketBytes = 1200;
} // namespace effect_limits

struct EffectRecord {
    uint8_t type = 0;
    uint8_t priority = 0;
    std::vector<Slot> slots;
};

struct EffectPacket {
    uint8_t playerId = 0; // stamped by the server (byte 1: StampPlayerId works)
    int16_t scene = -1;
    bool cinema = false;  // made by the cutscene its sender shows: only who watches it creates them
    std::vector<EffectRecord> effects;
};

// Packets of at most kPacketBytes and kEffects each. Effects with a bad type or too many slots are left out.
std::vector<std::vector<uint8_t>> EncodeEffects(const EffectPacket& p);
// False unless the buffer is exactly one well-formed packet within every limit.
bool DecodeEffects(const uint8_t* data, size_t size, EffectPacket& out);

} // namespace coop

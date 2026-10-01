#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace coop {

constexpr uint8_t kEponaActive = 1u << 0;
constexpr uint8_t kEponaOwnerMounted = 1u << 1;
constexpr uint8_t kEponaPassengerPresent = 1u << 2;
constexpr size_t kMaxEponasPerPacket = 255;

struct EponaKey {
    uint8_t ownerPlayerId = 0;
    uint32_t callSequence = 0;

    bool operator==(const EponaKey& other) const {
        return ownerPlayerId == other.ownerPlayerId && callSequence == other.callSequence;
    }

    bool operator<(const EponaKey& other) const {
        return ownerPlayerId < other.ownerPlayerId ||
               (ownerPlayerId == other.ownerPlayerId && callSequence < other.callSequence);
    }
};

struct EponaRot {
    int16_t x = 0;
    int16_t y = 0;
    int16_t z = 0;
};

struct EponaState {
    uint32_t callSequence = 0;
    bool active = true;
    float pos[3] = { 0.f, 0.f, 0.f };
    EponaRot rot;
    float speed = 0.f;
    uint8_t action = 0;
    uint8_t animation = 0;
    bool ownerMounted = false;
    bool passengerPresent = false;
};

struct EponaPacket {
    uint8_t ownerPlayerId = 0;
    uint16_t seq = 0;
    int16_t sceneId = -1;
    std::vector<EponaState> horses;
};

bool SanitizeEponaPacket(EponaPacket& packet);
std::vector<uint8_t> EncodeEponaState(const EponaPacket& packet);
bool DecodeEponaState(const uint8_t* data, size_t size, EponaPacket& out);
bool StampEponaOwnerId(uint8_t* data, size_t size, uint8_t ownerPlayerId);
size_t EponaWireSize(size_t horseCount = 0);

} // namespace coop

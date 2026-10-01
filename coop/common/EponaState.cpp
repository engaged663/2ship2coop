#include "EponaState.h"

#include "ByteStream.h"
#include "Protocol.h"
#include "StreamIds.h"

#include <cmath>
#include <unordered_set>

namespace coop {

namespace {

constexpr float kWorldLimit = 60000.f;
constexpr float kValueLimit = 1.0e6f;
constexpr size_t kHeaderSize = 1 + 1 + 2 + 2 + 1;
constexpr size_t kHorseSize = 4 + 1 + 12 + 6 + 4 + 1 + 1;

bool Finite(float value, float limit) {
    return std::isfinite(value) && std::fabs(value) <= limit;
}

} // namespace

bool SanitizeEponaPacket(EponaPacket& packet) {
    if (packet.sceneId < 0 || packet.sceneId > 0x7FFF || packet.horses.size() > kMaxEponasPerPacket) {
        return false;
    }
    std::unordered_set<uint32_t> sequences;
    for (EponaState& horse : packet.horses) {
        if (!horse.active || horse.callSequence == 0 || !sequences.insert(horse.callSequence).second) {
            return false;
        }
        if (!Finite(horse.pos[0], kWorldLimit) || !Finite(horse.pos[1], kWorldLimit) ||
            !Finite(horse.pos[2], kWorldLimit) || !Finite(horse.speed, kValueLimit)) {
            return false;
        }
    }
    return true;
}

std::vector<uint8_t> EncodeEponaState(const EponaPacket& packet) {
    if (packet.horses.size() > kMaxEponasPerPacket) {
        return {};
    }
    Writer writer;
    writer.U8(kStreamEponaState);
    writer.U8(packet.ownerPlayerId);
    writer.U16(packet.seq);
    writer.S16(packet.sceneId);
    writer.U8((uint8_t)packet.horses.size());
    for (const EponaState& horse : packet.horses) {
        uint8_t flags = (horse.active ? kEponaActive : 0) | (horse.ownerMounted ? kEponaOwnerMounted : 0) |
                        (horse.passengerPresent ? kEponaPassengerPresent : 0);
        writer.U32(horse.callSequence);
        writer.U8(flags);
        writer.F32(horse.pos[0]);
        writer.F32(horse.pos[1]);
        writer.F32(horse.pos[2]);
        writer.S16(horse.rot.x);
        writer.S16(horse.rot.y);
        writer.S16(horse.rot.z);
        writer.F32(horse.speed);
        writer.U8(horse.action);
        writer.U8(horse.animation);
    }
    return writer.Take();
}

bool DecodeEponaState(const uint8_t* data, size_t size, EponaPacket& out) {
    if (data == nullptr || size < kHeaderSize || data[0] != kStreamEponaState) {
        return false;
    }
    uint8_t count = data[6];
    if (count > kMaxEponasPerPacket || size != EponaWireSize(count)) {
        return false;
    }
    Reader reader(data, size);
    uint8_t type = 0;
    EponaPacket decoded;
    uint8_t countRead = 0;
    if (!reader.U8(type) || !reader.U8(decoded.ownerPlayerId) || !reader.U16(decoded.seq) ||
        !reader.S16(decoded.sceneId) || !reader.U8(countRead) || countRead != count) {
        return false;
    }
    decoded.horses.reserve(count);
    for (uint8_t i = 0; i < count; i++) {
        EponaState horse;
        uint8_t flags = 0;
        if (!reader.U32(horse.callSequence) || !reader.U8(flags) || !reader.F32(horse.pos[0]) ||
            !reader.F32(horse.pos[1]) || !reader.F32(horse.pos[2]) || !reader.S16(horse.rot.x) ||
            !reader.S16(horse.rot.y) || !reader.S16(horse.rot.z) || !reader.F32(horse.speed) ||
            !reader.U8(horse.action) || !reader.U8(horse.animation)) {
            return false;
        }
        if ((flags & ~(kEponaActive | kEponaOwnerMounted | kEponaPassengerPresent)) != 0) {
            return false;
        }
        horse.active = (flags & kEponaActive) != 0;
        horse.ownerMounted = (flags & kEponaOwnerMounted) != 0;
        horse.passengerPresent = (flags & kEponaPassengerPresent) != 0;
        decoded.horses.push_back(horse);
    }
    if (!SanitizeEponaPacket(decoded)) {
        return false;
    }
    out = std::move(decoded);
    return true;
}

bool StampEponaOwnerId(uint8_t* data, size_t size, uint8_t ownerPlayerId) {
    if (data == nullptr || size < 2 || data[0] != kStreamEponaState) {
        return false;
    }
    data[1] = ownerPlayerId;
    return true;
}

size_t EponaWireSize(size_t horseCount) {
    return kHeaderSize + horseCount * kHorseSize;
}

} // namespace coop

#include "TestMain.h"

#include "common/EponaState.h"
#include "common/StreamIds.h"

#include <cmath>
#include <limits>

using namespace coop;

namespace {

EponaState Horse(uint32_t sequence) {
    EponaState horse;
    horse.callSequence = sequence;
    horse.pos[0] = 10.5f;
    horse.pos[1] = -2.0f;
    horse.pos[2] = 300.0f;
    horse.rot = { 100, -200, 300 };
    horse.speed = 4.25f;
    horse.action = 7;
    horse.animation = 9;
    horse.ownerMounted = true;
    horse.passengerPresent = false;
    return horse;
}

EponaPacket Packet(std::initializer_list<uint32_t> sequences) {
    EponaPacket packet;
    packet.ownerPlayerId = 2;
    packet.seq = 77;
    packet.sceneId = 0x2D;
    for (uint32_t sequence : sequences) {
        packet.horses.push_back(Horse(sequence));
    }
    return packet;
}

} // namespace

TEST_CASE(EponaStateRoundTrip) {
    EponaPacket source = Packet({ 11 });
    source.horses[0].ownerMounted = false;
    source.horses[0].passengerPresent = true;

    auto bytes = EncodeEponaState(source);
    CHECK(!bytes.empty());
    CHECK_EQ(bytes[0], kStreamEponaState);
    CHECK_EQ(bytes.size(), EponaWireSize(source.horses.size()));

    EponaPacket decoded;
    CHECK(DecodeEponaState(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.ownerPlayerId, (uint8_t)2);
    CHECK_EQ(decoded.seq, (uint16_t)77);
    CHECK_EQ(decoded.sceneId, (int16_t)0x2D);
    CHECK_EQ(decoded.horses.size(), (size_t)1);
    CHECK_EQ(decoded.horses[0].callSequence, (uint32_t)11);
    CHECK_EQ(decoded.horses[0].pos[1], -2.0f);
    CHECK_EQ(decoded.horses[0].rot.y, (int16_t)-200);
    CHECK_EQ(decoded.horses[0].speed, 4.25f);
    CHECK(!decoded.horses[0].ownerMounted);
    CHECK(decoded.horses[0].passengerPresent);
}

TEST_CASE(EponaStateKeepsMultipleCallsForOneOwner) {
    EponaPacket source = Packet({ 1, 2 });
    auto bytes = EncodeEponaState(source);
    EponaPacket decoded;
    CHECK(DecodeEponaState(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.horses.size(), (size_t)2);
    CHECK_EQ(decoded.horses[0].callSequence, (uint32_t)1);
    CHECK_EQ(decoded.horses[1].callSequence, (uint32_t)2);
}

TEST_CASE(EponaStateKeysIncludeOwner) {
    EponaPacket first = Packet({ 5 });
    EponaPacket second = Packet({ 5 });
    second.ownerPlayerId = 3;
    auto firstBytes = EncodeEponaState(first);
    auto secondBytes = EncodeEponaState(second);
    EponaPacket firstDecoded;
    EponaPacket secondDecoded;
    CHECK(DecodeEponaState(firstBytes.data(), firstBytes.size(), firstDecoded));
    CHECK(DecodeEponaState(secondBytes.data(), secondBytes.size(), secondDecoded));
    CHECK_EQ(firstDecoded.ownerPlayerId, (uint8_t)2);
    CHECK_EQ(secondDecoded.ownerPlayerId, (uint8_t)3);
    CHECK_EQ(firstDecoded.horses[0].callSequence, secondDecoded.horses[0].callSequence);
}

TEST_CASE(EponaStateRejectsInvalidPackets) {
    EponaPacket nan = Packet({ 1 });
    nan.horses[0].pos[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(!SanitizeEponaPacket(nan));

    EponaPacket infinity = Packet({ 1 });
    infinity.horses[0].speed = std::numeric_limits<float>::infinity();
    CHECK(!SanitizeEponaPacket(infinity));

    EponaPacket farAway = Packet({ 1 });
    farAway.horses[0].pos[2] = 60001.0f;
    CHECK(!SanitizeEponaPacket(farAway));

    EponaPacket wrongScene = Packet({ 1 });
    wrongScene.sceneId = -1;
    CHECK(!SanitizeEponaPacket(wrongScene));

    EponaPacket duplicate = Packet({ 1, 1 });
    CHECK(!SanitizeEponaPacket(duplicate));
}

TEST_CASE(EponaStateRejectsMalformedWire) {
    auto bytes = EncodeEponaState(Packet({ 1 }));
    EponaPacket decoded;
    CHECK(!DecodeEponaState(bytes.data(), bytes.size() - 1, decoded));
    bytes.push_back(0);
    CHECK(!DecodeEponaState(bytes.data(), bytes.size(), decoded));
    bytes[0] = 99;
    CHECK(!DecodeEponaState(bytes.data(), bytes.size() - 1, decoded));
}

TEST_CASE(EponaStateOwnerStampWritesHeader) {
    auto bytes = EncodeEponaState(Packet({ 1 }));
    CHECK(StampEponaOwnerId(bytes.data(), bytes.size(), 4));
    EponaPacket decoded;
    CHECK(DecodeEponaState(bytes.data(), bytes.size(), decoded));
    CHECK_EQ(decoded.ownerPlayerId, (uint8_t)4);
    CHECK(!StampEponaOwnerId(bytes.data(), 1, 4));
}

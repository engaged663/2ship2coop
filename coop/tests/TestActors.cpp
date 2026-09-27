// Sub-project C: the actor stream format, room authority and the relay rules of enemies, hits and drops.
#include "TestWorld.h"

#include "common/ActorState.h"

#include <limits>
#include <set>

using namespace coop;
using namespace coop_test;

namespace {

ActorRecord Record(uint16_t key, int joints = 20) {
    ActorRecord r;
    r.key = key;
    r.actorId = 0x33;
    r.params = -2;
    r.visible = true;
    r.pos[0] = 10.f + key;
    r.pos[1] = -5.f;
    r.pos[2] = 300.f;
    r.rot = { 1, 2, 3 };
    r.worldRotY = 4;
    r.scale[0] = r.scale[1] = r.scale[2] = 0.01f;
    r.health = 3;
    r.colorFilterParams = 0x4000;
    r.colorFilterTimer = 5;
    r.shadowAlpha = 255;
    for (int i = 0; i < joints; i++) {
        r.joints.push_back({ (int16_t)i, (int16_t)-i, (int16_t)(i * 2) });
    }
    r.colliders.push_back({ ActorCollider::kAc | ActorCollider::kOc, { 7, 8, 9 } });
    r.sfx.push_back(0x3812);
    r.extras = { 1, 2, 3 };
    return r;
}

ActorPacket Frame(int records, int joints = 20) {
    ActorPacket p;
    p.scene = 0x2D;
    p.room = 1;
    p.seq = 77;
    p.gone = { 5, 6 };
    for (int i = 0; i < records; i++) {
        p.actors.push_back(Record((uint16_t)(100 + i), joints));
    }
    return p;
}

bool SameVec(const Vec3s16& a, const Vec3s16& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool SameRecord(const ActorRecord& a, const ActorRecord& b) {
    bool same = a.key == b.key && a.actorId == b.actorId && a.params == b.params && a.visible == b.visible &&
                SameVec(a.rot, b.rot) && a.worldRotY == b.worldRotY && a.health == b.health &&
                a.colorFilterParams == b.colorFilterParams && a.colorFilterTimer == b.colorFilterTimer &&
                a.shadowAlpha == b.shadowAlpha && a.joints.size() == b.joints.size() &&
                a.colliders.size() == b.colliders.size() && a.sfx == b.sfx && a.extras == b.extras;
    for (int i = 0; same && i < 3; i++) {
        same = a.pos[i] == b.pos[i] && a.scale[i] == b.scale[i];
    }
    for (size_t i = 0; same && i < a.joints.size(); i++) {
        same = SameVec(a.joints[i], b.joints[i]);
    }
    for (size_t i = 0; same && i < a.colliders.size(); i++) {
        same = a.colliders[i].flags == b.colliders[i].flags && SameVec(a.colliders[i].dimPos, b.colliders[i].dimPos);
    }
    return same;
}

} // namespace

TEST_CASE(ActorPacketRoundTrip) {
    ActorPacket frame = Frame(3);
    auto packets = EncodeActorPackets(frame);
    CHECK_EQ(packets.size(), (size_t)1);
    ActorPacket back;
    CHECK(DecodeActorPacket(packets[0].data(), packets[0].size(), back));
    CHECK_EQ(back.scene, (int16_t)0x2D);
    CHECK_EQ(back.room, (int8_t)1);
    CHECK_EQ(back.seq, (uint16_t)77);
    CHECK_EQ(back.part, (uint8_t)0);
    CHECK_EQ(back.parts, (uint8_t)1);
    CHECK_EQ(back.total, (uint16_t)3);
    CHECK(back.gone == frame.gone);
    CHECK_EQ(back.actors.size(), (size_t)3);
    for (size_t i = 0; i < 3; i++) {
        CHECK(SameRecord(back.actors[i], frame.actors[i]));
    }
}

TEST_CASE(ActorPacketSplitsBigRooms) {
    ActorPacket frame = Frame(12, 32);
    auto packets = EncodeActorPackets(frame);
    CHECK(packets.size() > 1);
    std::set<uint16_t> keys;
    for (size_t i = 0; i < packets.size(); i++) {
        CHECK(packets[i].size() <= actor_limits::kPacketBytes);
        ActorPacket part;
        CHECK(DecodeActorPacket(packets[i].data(), packets[i].size(), part));
        CHECK_EQ(part.part, (uint8_t)i);
        CHECK_EQ(part.parts, (uint8_t)packets.size());
        CHECK_EQ(part.total, (uint16_t)12);
        CHECK_EQ(part.gone.size(), i == 0 ? (size_t)2 : (size_t)0);
        for (const ActorRecord& a : part.actors) {
            keys.insert(a.key);
        }
    }
    CHECK_EQ(keys.size(), (size_t)12);
}

TEST_CASE(ActorPacketCutsOverlongLists) {
    ActorPacket frame = Frame(1, 40);
    frame.actors[0].sfx = { 1, 2, 3, 4, 5, 6 };
    auto packets = EncodeActorPackets(frame);
    ActorPacket back;
    CHECK(DecodeActorPacket(packets[0].data(), packets[0].size(), back));
    CHECK_EQ(back.actors[0].joints.size(), (size_t)actor_limits::kJoints);
    CHECK_EQ(back.actors[0].sfx.size(), (size_t)actor_limits::kSfx);
}

TEST_CASE(ActorPacketRejectsGarbage) {
    auto good = EncodeActorPackets(Frame(2))[0];
    ActorPacket out;
    for (size_t n = 0; n < good.size(); n++) {
        CHECK(!DecodeActorPacket(good.data(), n, out));
    }
    auto longer = good;
    longer.push_back(0);
    CHECK(!DecodeActorPacket(longer.data(), longer.size(), out));
    auto wrongType = good;
    wrongType[0] = kStreamPlayerState;
    CHECK(!DecodeActorPacket(wrongType.data(), wrongType.size(), out));

    ActorPacket badRoom = Frame(1);
    badRoom.room = 99;
    auto p = EncodeActorPackets(badRoom)[0];
    CHECK(!DecodeActorPacket(p.data(), p.size(), out));
    ActorPacket nan = Frame(1);
    nan.actors[0].pos[1] = std::numeric_limits<float>::quiet_NaN();
    p = EncodeActorPackets(nan)[0];
    CHECK(!DecodeActorPacket(p.data(), p.size(), out));
    ActorPacket far = Frame(1);
    far.actors[0].scale[0] = 1e9f;
    p = EncodeActorPackets(far)[0];
    CHECK(!DecodeActorPacket(p.data(), p.size(), out));

    // 33 joints written by hand: the count byte sits right after shadowAlpha.
    auto bytes = EncodeActorPackets(Frame(1, 0))[0];
    // tail after it: [nCol=1][flags + 3 s16] [nSfx=1][u16] [nExtras=3][3 bytes] = 15 bytes
    size_t jointCount = bytes.size() - 16;
    CHECK_EQ(bytes[jointCount], (uint8_t)0);
    bytes[jointCount] = 33;
    CHECK(!DecodeActorPacket(bytes.data(), bytes.size(), out));
}

TEST_CASE(ActorPacketStampAndPeek) {
    auto p = EncodeActorPackets(Frame(1))[0];
    CHECK(StampPlayerId(p.data(), p.size(), 3));
    ActorPacket out;
    CHECK(DecodeActorPacket(p.data(), p.size(), out));
    CHECK_EQ(out.playerId, (uint8_t)3);
    int16_t scene = 0;
    int8_t room = 0;
    CHECK(PeekActorHeader(p.data(), p.size(), scene, room));
    CHECK_EQ(scene, (int16_t)0x2D);
    CHECK_EQ(room, (int8_t)1);
    CHECK(!PeekActorHeader(p.data(), 4, scene, room));
}

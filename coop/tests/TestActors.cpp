// Sub-project C: the actor stream format, room authority and the relay rules of enemies, hits and drops.
#include "TestWorld.h"

#include "common/ActorState.h"
#include "server/World/RoomAuthority.h"

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
    r.focus[1] = 20.f;
    r.rot = { 1, 2, 3 };
    r.worldRotY = 4;
    r.scale[0] = r.scale[1] = r.scale[2] = 0.01f;
    r.health = 3;
    r.colorFilterParams = 0x4000;
    r.colorFilterTimer = 5;
    r.shadowAlpha = 255;
    r.yOffset = -2000.f;
    r.shadowScale = 12.f;
    r.flags = 0x205;
    for (int i = 0; i < joints; i++) {
        r.joints.push_back({ (int16_t)i, (int16_t)-i, (int16_t)(i * 2) });
    }
    r.colliders.push_back({ ActorCollider::kAc | ActorCollider::kOc, { 7, 8, 9 } });
    r.sfx.push_back(0x3812);
    r.loopSfx = 0x3001;
    r.loopSfxFlags = 2;
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
                a.shadowAlpha == b.shadowAlpha && a.yOffset == b.yOffset && a.shadowScale == b.shadowScale && a.flags == b.flags &&
                a.joints.size() == b.joints.size() &&
                a.colliders.size() == b.colliders.size() && a.sfx == b.sfx && a.extras == b.extras &&
                a.loopSfx == b.loopSfx && a.loopSfxFlags == b.loopSfxFlags;
    for (int i = 0; same && i < 3; i++) {
        same = a.pos[i] == b.pos[i] && a.focus[i] == b.focus[i] && a.scale[i] == b.scale[i];
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
    // tail after it: [nCol=1][flags + 3 s16] [nSfx=1][u16] [loop u16 + u8] [nExtras=3][3 bytes] = 18 bytes
    size_t jointCount = bytes.size() - 19;
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

static uint8_t Owner(const std::map<server::RoomKey, uint8_t>& owners, int16_t scene, int8_t room) {
    auto it = owners.find({ scene, room });
    return it == owners.end() ? 0 : it->second;
}

TEST_CASE(AuthorityIsTheOldestInTheRoom) {
    using server::AuthMember;
    std::vector<AuthMember> m = { { 1, 0x2D, 0, false, 500 }, { 2, 0x2D, 0, false, 100 }, { 3, 0x2D, 0, false, 900 },
                                  { 4, 0x2D, 2, false, 800 } };
    auto owners = server::ComputeAuthority(m);
    CHECK_EQ(owners.size(), (size_t)2);
    CHECK_EQ(Owner(owners, 0x2D, 0), (uint8_t)2);
    CHECK_EQ(Owner(owners, 0x2D, 2), (uint8_t)4);
    m.push_back({ 1, 0x10, 0, false, 100 }); // same id elsewhere does not matter; ties go to the lower id
    m[2].sinceMs = 100;
    owners = server::ComputeAuthority(m);
    CHECK_EQ(Owner(owners, 0x2D, 0), (uint8_t)2);
    CHECK_EQ(Owner(owners, 0x10, 0), (uint8_t)1);
}

TEST_CASE(AuthorityPassesWhenOwnerLeavesOrIsBusy) {
    using server::AuthMember;
    std::vector<AuthMember> m = { { 1, 0x2D, 0, false, 100 }, { 2, 0x2D, 0, false, 200 }, { 3, 0x2D, 0, false, 300 } };
    m.erase(m.begin());
    CHECK_EQ(Owner(server::ComputeAuthority(m), 0x2D, 0), (uint8_t)2);
    m[0].busy = true;
    CHECK_EQ(Owner(server::ComputeAuthority(m), 0x2D, 0), (uint8_t)3);
    m[1].busy = true;
    CHECK_EQ(Owner(server::ComputeAuthority(m), 0x2D, 0), (uint8_t)2); // everyone busy: the oldest keeps it
}

TEST_CASE(AuthorityIgnoresNegativeRooms) {
    using server::AuthMember;
    std::vector<AuthMember> m = { { 1, 0x2D, -1, false, 100 }, { 2, -1, 0, false, 100 } };
    CHECK(server::ComputeAuthority(m).empty());
}

// ---- Server: authority and relays ----

namespace {

constexpr int16_t kScene = 0x2D;

// Two or three players in the world, all in scene kScene room 0 (the first one arrived first).
struct Room {
    TestServer s;
    std::unique_ptr<TestClient> a, b;
    explicit Room(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        a->SendState(kScene, 0);
        s.PumpFor(30);
        b->SendState(kScene, 0);
        s.PumpFor(60);
    }
};

// The latest "auth" for scene (older ones dropped).
std::optional<json> LastAuth(TestServer& s, TestClient& c, int timeoutMs = 500) {
    s.PumpFor(timeoutMs);
    std::optional<json> last;
    while (auto ev = c.TakeEvent("auth")) {
        last = ev;
    }
    return last;
}

uint8_t AuthOwner(const json& auth, int room) {
    for (const json& pair : auth["rooms"]) {
        if (pair[0].get<int>() == room) {
            return (uint8_t)pair[1].get<int>();
        }
    }
    return 0;
}

void SendActors(TestClient& c, int16_t scene, int8_t room) {
    ActorPacket frame = Frame(1);
    frame.scene = scene;
    frame.room = room;
    auto packets = EncodeActorPackets(frame);
    c.transport.Send(c.peer, kChannelStream, packets[0].data(), packets[0].size());
}

bool GetsActors(TestServer& s, TestClient& c, uint8_t* fromId = nullptr, int timeoutMs = 300) {
    bool got = c.WaitUntil(s, timeoutMs, [&] { return !c.rawStreams.empty(); });
    if (got && fromId != nullptr) {
        ActorPacket p;
        *fromId = DecodeActorPacket(c.rawStreams.front().data(), c.rawStreams.front().size(), p) ? p.playerId : 0;
    }
    c.rawStreams.clear();
    return got;
}

json Hit(int16_t scene = kScene, int room = 0) {
    return { { "t", "hit" },     { "scene", scene },    { "room", room },    { "key", 3 },  { "col", 0 },
             { "elem", 0 },      { "dmgFlags", 0x200 }, { "effect", 0 },     { "damage", 2 }, { "hitEffect", 0 },
             { "pos", Arr(1.0, 2.0, 3.0) }, { "attackerId", 0 }, { "form", 4 } };
}

json Hurt(uint8_t to, int damage = 8) {
    return { { "t", "hurt" }, { "to", to },       { "kind", "col" },  { "dmgFlags", 0x1 },
             { "effect", 0 }, { "damage", damage }, { "hitEffect", 0 }, { "pos", Arr(0.0, 0.0, 0.0) } };
}

} // namespace

TEST_CASE(AuthAnnouncedToTheScene) {
    Room r;
    auto authA = LastAuth(r.s, *r.a);
    auto authB = LastAuth(r.s, *r.b, 0);
    CHECK(authA.has_value() && authB.has_value());
    CHECK_EQ(GetInt(*authA, "scene"), (int64_t)kScene);
    CHECK_EQ(AuthOwner(*authA, 0), r.a->id);
    CHECK_EQ(AuthOwner(*authB, 0), r.a->id);
}

TEST_CASE(AuthorityPassesWhenOwnerLeavesTheWorld) {
    Room r;
    LastAuth(r.s, *r.b);
    r.a->Send({ { "t", "world_leave" } });
    auto authB = LastAuth(r.s, *r.b);
    CHECK(authB.has_value());
    CHECK_EQ(AuthOwner(*authB, 0), r.b->id);
    auto authA = LastAuth(r.s, *r.a, 0);
    CHECK(authA.has_value());
    CHECK_EQ(GetInt(*authA, "scene"), (int64_t)-1); // it no longer takes part
}

TEST_CASE(BusyOwnerHandsOver) {
    Room r;
    LastAuth(r.s, *r.b);
    r.a->Send({ { "t", "loc" }, { "scene", kScene }, { "room", 0 }, { "entrance", 0 }, { "sceneName", "x" }, { "busy", true } });
    auto authB = LastAuth(r.s, *r.b);
    CHECK(authB.has_value());
    CHECK_EQ(AuthOwner(*authB, 0), r.b->id);
    r.a->Send({ { "t", "loc" }, { "scene", kScene }, { "room", 0 }, { "entrance", 0 }, { "sceneName", "x" }, { "busy", false } });
    authB = LastAuth(r.s, *r.b);
    CHECK(authB.has_value());
    CHECK_EQ(AuthOwner(*authB, 0), r.a->id); // not busy any more: the oldest gets it back
}

TEST_CASE(ActorStreamOnlyFromAuthority) {
    Room r;
    LastAuth(r.s, *r.a);
    SendActors(*r.a, kScene, 0);
    uint8_t from = 0;
    CHECK(GetsActors(r.s, *r.b, &from));
    CHECK_EQ(from, r.a->id);
    CHECK(!GetsActors(r.s, *r.a, nullptr, 100)); // not echoed
    SendActors(*r.b, kScene, 0);
    CHECK(!GetsActors(r.s, *r.a));
    SendActors(*r.a, kScene, 5); // a room nobody is in has no owner
    CHECK(!GetsActors(r.s, *r.b));
}

TEST_CASE(ActorTrafficIgnoresPlayersOutsideTheWorld) {
    Room r;
    auto c = Join(r.s, "Cid"); // connected, same scene, playing its own save
    c->SendState(kScene, 0);
    LastAuth(r.s, *r.a);
    CHECK(!c->TakeEvent("auth").has_value());
    SendActors(*r.a, kScene, 0);
    CHECK(GetsActors(r.s, *r.b));
    CHECK(!GetsActors(r.s, *c, nullptr, 100));
    c->Send(Hit());
    CHECK(!r.a->WaitFor("hit", r.s, 200).has_value());
    r.a->Send(Hurt(c->id));
    CHECK(!c->WaitFor("hurt", r.s, 200).has_value());
}

TEST_CASE(HitGoesOnlyToTheAuthority) {
    Room r;
    auto c = Join(r.s, "Cid");
    EnterWorld(r.s, *c);
    c->SendState(0x10, 0); // in the world, another scene
    LastAuth(r.s, *r.a);
    r.b->Send(Hit());
    auto hit = r.a->WaitFor("hit", r.s);
    CHECK(hit.has_value());
    CHECK_EQ(GetInt(*hit, "from"), (int64_t)r.b->id);
    CHECK_EQ(GetInt(*hit, "key"), (int64_t)3);
    CHECK(!c->WaitFor("hit", r.s, 100).has_value());
    r.a->Send(Hit()); // the owner hitting its own enemy: nobody to tell
    CHECK(!r.b->WaitFor("hit", r.s, 200).has_value());
    json bad = Hit();
    bad["col"] = 9;
    r.b->Send(bad);
    CHECK(!r.a->WaitFor("hit", r.s, 200).has_value());
}

TEST_CASE(HurtOnlyFromAuthority) {
    Room r;
    LastAuth(r.s, *r.a);
    r.b->Send(Hurt(r.a->id));
    CHECK(!r.a->WaitFor("hurt", r.s, 200).has_value());
    r.a->Send(Hurt(r.b->id));
    auto hurt = r.b->WaitFor("hurt", r.s);
    CHECK(hurt.has_value());
    CHECK_EQ(GetInt(*hurt, "from"), (int64_t)r.a->id);
    CHECK_EQ(GetInt(*hurt, "damage"), (int64_t)8);
    r.a->Send(Hurt(r.a->id)); // itself
    CHECK(!r.a->WaitFor("hurt", r.s, 200).has_value());
}

TEST_CASE(HurtDamageIsClamped) {
    Room r;
    LastAuth(r.s, *r.a);
    r.a->Send(Hurt(r.b->id, 255));
    auto hurt = r.b->WaitFor("hurt", r.s);
    CHECK(hurt.has_value());
    CHECK_EQ(GetInt(*hurt, "damage"), (int64_t)kMaxHurtDamage);
}

TEST_CASE(DropRelayedToTheScene) {
    Room r;
    LastAuth(r.s, *r.a);
    json drop = { { "t", "drop" }, { "scene", kScene }, { "room", 0 }, { "pos", Arr(5.0, 6.0, 7.0) },
                  { "params", 0x10 }, { "fn", 0 } };
    r.a->Send(drop);
    auto got = r.b->WaitFor("drop", r.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)r.a->id);
    r.b->Send(drop); // not the owner
    CHECK(!r.a->WaitFor("drop", r.s, 200).has_value());
}

TEST_CASE(SharedEnemiesOffSendsNothing) {
    server::ServerConfig cfg;
    cfg.sharedEnemies = false;
    Room r(cfg);
    CHECK(!LastAuth(r.s, *r.a).has_value());
    SendActors(*r.a, kScene, 0);
    CHECK(!GetsActors(r.s, *r.b));
}

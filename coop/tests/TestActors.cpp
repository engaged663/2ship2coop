// Sub-project C: room authority and the relay rules of actors, hits and drops (the C1 format left with D3).
#include "TestWorld.h"

#include "common/ActorImage.h"
#include "server/World/RoomAuthority.h"

#include <limits>
#include <set>

using namespace coop;
using namespace coop_test;

namespace {

ActorImageRecord Record(uint16_t key) {
    ActorImageRecord r;
    r.key = key;
    r.actorId = 0x33;
    r.spans.push_back({ 0, 0, { { SlotKind::Raw, 0x1111 } } });
    return r;
}

ActorImagePacket Frame(int records) {
    ActorImagePacket p;
    p.scene = 0x2D;
    p.room = 1;
    p.seq = 77;
    p.gone = { 5, 6 };
    for (int i = 0; i < records; i++) {
        p.records.push_back(Record((uint16_t)(100 + i)));
    }
    return p;
}

} // namespace

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
    ActorImagePacket frame = Frame(1);
    frame.scene = scene;
    frame.room = room;
    auto packets = EncodeActorImage(frame);
    c.transport.Send(c.peer, kChannelStream, packets[0].data(), packets[0].size());
}

bool GetsActors(TestServer& s, TestClient& c, uint8_t* fromId = nullptr, int timeoutMs = 300) {
    bool got = c.WaitUntil(s, timeoutMs, [&] { return !c.rawStreams.empty(); });
    if (got && fromId != nullptr) {
        ActorImagePacket p;
        *fromId = DecodeActorImage(c.rawStreams.front().data(), c.rawStreams.front().size(), p) ? p.playerId : 0;
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

json EponaCall(uint32_t sequence, int16_t scene = kScene) {
    return { { "t", "epona_call" }, { "scene", scene }, { "horse", sequence },
             { "pos", Arr(10.0, 20.0, 30.0) }, { "rot", Arr(0.0, 100.0, 200.0) } };
}

// Epona's Song: every game in the scene hears the call (each brings its own horse); other scenes and hosts do not.
TEST_CASE(EponaCallRelayedToTheScene) {
    Room r;
    LastAuth(r.s, *r.b);
    json call = EponaCall(1);
    r.a->Send(call);
    auto got = r.b->WaitFor("epona_call", r.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)r.a->id);
    CHECK_EQ(GetInt(*got, "horse"), (int64_t)1);
    CHECK(!r.a->WaitFor("epona_call", r.s, 200).has_value()); // not echoed to the caller
    json bad = call;
    bad["scene"] = 0x10; // not their scene
    r.a->Send(bad);
    CHECK(!r.b->WaitFor("epona_call", r.s, 150).has_value());
}

TEST_CASE(EponaCallsKeepIndependentSequences) {
    Room r;
    LastAuth(r.s, *r.b);
    r.a->Send(EponaCall(1));
    r.a->Send(EponaCall(2));
    auto first = r.b->WaitFor("epona_call", r.s);
    auto second = r.b->WaitFor("epona_call", r.s);
    CHECK(first.has_value() && second.has_value());
    CHECK_EQ(GetInt(*first, "horse"), (int64_t)1);
    CHECK_EQ(GetInt(*second, "horse"), (int64_t)2);
}

TEST_CASE(EponaCallRejectsReusedSequence) {
    Room r;
    LastAuth(r.s, *r.b);
    r.a->Send(EponaCall(4));
    CHECK(r.b->WaitFor("epona_call", r.s).has_value());
    r.a->Send(EponaCall(4));
    CHECK(!r.b->WaitFor("epona_call", r.s, 200).has_value());
    r.a->Send(EponaCall(3));
    CHECK(!r.b->WaitFor("epona_call", r.s, 200).has_value());
}

TEST_CASE(EponaCallRejectsMalformedState) {
    Room r;
    LastAuth(r.s, *r.b);
    json missingHorse = EponaCall(1);
    missingHorse.erase("horse");
    r.a->Send(missingHorse);
    json badPosition = EponaCall(2);
    badPosition["pos"] = Arr(1.0, 2.0);
    r.a->Send(badPosition);
    CHECK(!r.b->WaitFor("epona_call", r.s, 250).has_value());
}

TEST_CASE(EponaStateRelayedWithOwnerAndSceneFilter) {
    Room r;
    LastAuth(r.s, *r.b);
    EponaPacket packet;
    packet.ownerPlayerId = 99;
    packet.seq = 3;
    packet.sceneId = kScene;
    packet.horses.push_back(EponaState{});
    packet.horses[0].callSequence = 4;
    packet.horses[0].pos[0] = 12.0f;
    packet.horses[0].ownerMounted = true;
    r.a->SendEpona(packet);
    auto got = r.b->WaitForEponaStream(r.s);
    CHECK(got.has_value());
    CHECK_EQ(got->ownerPlayerId, r.a->id);
    CHECK_EQ(got->sceneId, kScene);
    CHECK_EQ(got->horses.size(), (size_t)1);
    CHECK_EQ(got->horses[0].callSequence, (uint32_t)4);
    CHECK(got->horses[0].ownerMounted);

    packet.sceneId = 0x10;
    r.a->SendEpona(packet);
    CHECK(!r.b->WaitForEponaStream(r.s, 200).has_value());
}

TEST_CASE(EponaPassengerRelayedToOwner) {
    Room r;
    LastAuth(r.s, *r.b);
    r.a->Send(EponaCall(1));
    CHECK(r.b->WaitFor("epona_call", r.s).has_value());
    r.b->Send({ { "t", "epona_passenger" }, { "scene", kScene }, { "owner", r.a->id },
                { "horse", 1 }, { "mounted", true } });
    auto got = r.a->WaitFor("epona_passenger", r.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)r.b->id);
    CHECK_EQ(GetInt(*got, "owner"), (int64_t)r.a->id);
    CHECK(GetBool(*got, "mounted"));
    CHECK(!r.b->WaitFor("epona_passenger", r.s, 150).has_value());
}

TEST_CASE(EponaPassengerRejectsWrongOwnerScene) {
    Room r;
    LastAuth(r.s, *r.b);
    r.b->Send({ { "t", "epona_passenger" }, { "scene", 0x10 }, { "owner", r.a->id },
                { "horse", 1 }, { "mounted", true } });
    r.b->Send({ { "t", "epona_passenger" }, { "scene", kScene }, { "owner", r.b->id },
                { "horse", 1 }, { "mounted", true } });
    CHECK(!r.a->WaitFor("epona_passenger", r.s, 250).has_value());
}

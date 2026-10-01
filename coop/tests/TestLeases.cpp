// Sub-project D3: NPCs lent to the player next to them, and the relay rules that follow the lease.
#include "TestWorld.h"

#include "common/ActorImage.h"

using namespace coop;
using namespace coop_test;

namespace {

constexpr int16_t kScene = 0x6F;

server::ServerConfig Cfg() {
    server::ServerConfig cfg;
    cfg.leaseExpireMs = 1200; // short, so a test can wait it out (longer than the waits of one step)
    return cfg;
}

// Alice (arrived first: owns room 0), Bob and Carol, all in kScene room 0.
struct Town {
    TestServer s;
    std::unique_ptr<TestClient> a, b, c;
    Town() : s(Cfg()) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        c = Join(s, "Carol");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        EnterWorld(s, *c);
        a->SendState(kScene, 0);
        s.PumpFor(30);
        b->SendState(kScene, 0);
        c->SendState(kScene, 0);
        s.PumpFor(100);
    }
};

json LeaseReq(uint32_t key, double dist, bool talking = false, int room = 0, int16_t scene = kScene) {
    return { { "t", "lease_req" }, { "scene", scene }, { "room", room },
             { "key", key },       { "dist", dist },   { "talking", talking } };
}

// Who holds key in the latest "leases" c got (0: nobody).
uint8_t Holder(TestServer& s, TestClient& c, uint32_t key, int timeoutMs = 300) {
    s.PumpFor(timeoutMs);
    std::optional<json> last;
    while (auto ev = c.TakeEvent("leases")) {
        last = ev;
    }
    if (!last.has_value()) {
        return 0xFF; // no change announced
    }
    for (const json& l : (*last)["list"]) {
        if (l[1].get<uint32_t>() == key) {
            return (uint8_t)l[2].get<int>();
        }
    }
    return 0;
}

void SendImage(TestClient& c, int8_t room) {
    ActorImagePacket f;
    f.scene = kScene;
    f.room = room;
    f.seq = 1;
    ActorImageRecord r;
    r.key = 4;
    r.actorId = 0x10;
    f.records = { r };
    auto p = EncodeActorImage(f)[0];
    c.transport.Send(c.peer, kChannelStream, p.data(), p.size());
}

bool GetsImage(TestServer& s, TestClient& c, int timeoutMs = 300) {
    bool got = c.WaitUntil(s, timeoutMs, [&] { return !c.rawStreams.empty(); });
    c.rawStreams.clear();
    return got;
}

json HitOn(uint32_t key, uint32_t root) {
    return { { "t", "hit" },     { "scene", kScene },  { "room", 0 },       { "key", key },   { "root", root },
             { "col", 0 },       { "elem", 0 },        { "dmgFlags", 0x200 }, { "effect", 0 }, { "damage", 2 },
             { "hitEffect", 0 }, { "pos", Arr(1.0, 2.0, 3.0) }, { "attackerId", 0 }, { "form", 4 } };
}

} // namespace

TEST_CASE(LeaseGrantedWhenFree) {
    Town t;
    t.b->Send(LeaseReq(7, 150));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id); // everyone in the scene hears it
    CHECK_EQ(Holder(t.s, *t.c, 7, 0), t.b->id);
}

TEST_CASE(LeaseMovesToAClearlyCloserPlayer) {
    Town t;
    t.b->Send(LeaseReq(7, 300));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    t.c->Send(LeaseReq(7, 280)); // only a bit closer: stays
    CHECK_EQ(Holder(t.s, *t.a, 7), (uint8_t)0xFF);
    t.c->Send(LeaseReq(7, 100)); // clearly closer: moves
    CHECK_EQ(Holder(t.s, *t.a, 7), t.c->id);
}

TEST_CASE(LeaseStaysWhileTalking) {
    Town t;
    t.b->Send(LeaseReq(7, 300, true));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    t.c->Send(LeaseReq(7, 10));
    CHECK_EQ(Holder(t.s, *t.a, 7), (uint8_t)0xFF);
}

TEST_CASE(LeaseDroppedOrExpired) {
    Town t;
    t.b->Send(LeaseReq(7, 150));
    t.b->Send(LeaseReq(8, 150));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    t.b->Send({ { "t", "lease_drop" }, { "scene", kScene }, { "room", 0 }, { "key", 7 } });
    CHECK_EQ(Holder(t.s, *t.a, 7), (uint8_t)0);
    CHECK_EQ(Holder(t.s, *t.a, 8, 1500), (uint8_t)0); // 8 was never renewed
}

// A minigame's director keeps its NPC and props (up to kMaxLeasesPerPlayer) and asks for each twice a second: the
// budget must cover a full hand, or the server would drop renewals and the same props would expire again and again.
TEST_CASE(LeaseBudgetCoversAFullHand) {
    Town t;
    const uint32_t hand = (uint32_t)kMaxLeasesPerPlayer;
    for (uint32_t key = 1; key <= hand; key++) {
        t.b->Send(LeaseReq(key, 500, true));
    }
    CHECK_EQ(Holder(t.s, *t.a, hand), t.b->id);
    for (int renewal = 0; renewal < 2; renewal++) {
        for (uint32_t key = 1; key <= hand; key++) {
            t.b->Send(LeaseReq(key, 500, true));
        }
    }
    t.b->Send({ { "t", "lease_drop" }, { "scene", kScene }, { "room", 0 }, { "key", 1 } });
    CHECK_EQ(Holder(t.s, *t.a, 1), (uint8_t)0); // heard after three rounds of requests in a row
}

TEST_CASE(LeaseGoesWhenTheHolderLeaves) {
    Town t;
    t.b->Send(LeaseReq(7, 150));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    t.b->SendState(0x2D, 0); // another scene
    CHECK_EQ(Holder(t.s, *t.a, 7), (uint8_t)0);
    t.c->Send(LeaseReq(9, 150));
    CHECK_EQ(Holder(t.s, *t.a, 9), t.c->id);
    t.c.reset(); // gone
    CHECK_EQ(Holder(t.s, *t.a, 9), (uint8_t)0);
}

TEST_CASE(LeaseRequestsAreChecked) {
    Town t;
    t.b->Send(LeaseReq(7, 150, false, 0, 0x2D)); // not its scene
    t.b->Send(LeaseReq(7, 150, false, 99));      // impossible room
    t.b->Send(LeaseReq(7, -5));                  // impossible distance
    CHECK_EQ(Holder(t.s, *t.a, 7), (uint8_t)0xFF);
    // A runtime actor (Epona: the rider's) is lent to whoever asks first and only they renew it.
    t.b->Send(LeaseReq(0x80000001u, 150));
    CHECK_EQ(Holder(t.s, *t.a, 0x80000001u), t.b->id);
    t.c->Send(LeaseReq(0x80000001u, 10, true));
    // No new "leases" says c took it (0xFF: nothing changed; or b still holds it).
    uint8_t holder = Holder(t.s, *t.a, 0x80000001u);
    CHECK(holder == 0xFF || holder == t.b->id);
}

TEST_CASE(HitGoesToTheLessee) {
    Town t;
    t.b->Send(LeaseReq(7, 150));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    t.c->Send(HitOn(0x40000123u, 7)); // a child of the lent NPC
    auto hit = t.b->WaitFor("hit", t.s);
    CHECK(hit.has_value());
    CHECK_EQ(GetInt(*hit, "from"), (int64_t)t.c->id);
    CHECK(!t.a->WaitFor("hit", t.s, 150).has_value());
    t.c->Send(HitOn(3, 3)); // not lent: the room's owner
    CHECK(t.a->WaitFor("hit", t.s).has_value());
}

TEST_CASE(LesseeStreamIsRelayed) {
    Town t;
    SendImage(*t.b, 0); // neither owner nor lessee
    CHECK(!GetsImage(t.s, *t.a));
    t.b->Send(LeaseReq(7, 150));
    CHECK_EQ(Holder(t.s, *t.a, 7), t.b->id);
    SendImage(*t.b, 0);
    CHECK(GetsImage(t.s, *t.a));
    CHECK(GetsImage(t.s, *t.c, 50));
    SendImage(*t.a, 0); // the owner too
    CHECK(GetsImage(t.s, *t.b));
}

// A game showing its cutscene to others sends its cutscene actors too (spec grupos-limites §3), whoever owns the
// room: its frames pass while its "cinema" keeps coming.
TEST_CASE(CutsceneDirectorStreamIsRelayed) {
    Town t;
    SendImage(*t.b, 0); // neither owner nor lessee, no cutscene
    CHECK(!GetsImage(t.s, *t.a));
    t.b->Send({ { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 },
                { "scope", "scene" } });
    t.s.PumpFor(50);
    SendImage(*t.b, 0);
    CHECK(GetsImage(t.s, *t.a));
    t.s.PumpFor(kCinemaActorsMs + 200); // its camera stopped coming
    t.a->rawStreams.clear();
    SendImage(*t.b, 0);
    CHECK(!GetsImage(t.s, *t.a));
}

TEST_CASE(HostsNeverBorrow) {
    server::ServerConfig cfg = Cfg();
    cfg.hostToken = "secret";
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->SendState(kScene, 0);
    auto host = Connect(s);
    host->Send({ { "t", "hello" }, { "proto", kProtocolVersion }, { "nick", "#host" }, { "host", true },
                 { "token", "secret" } });
    CHECK(host->WaitFor("welcome", s).has_value());
    host->Send({ { "t", "world_enter" } });
    CHECK(host->WaitFor("world_full", s).has_value());
    host->SendState(kScene, 0);
    s.PumpFor(100);
    host->Send(LeaseReq(7, 10));
    CHECK_EQ(Holder(s, *a, 7), (uint8_t)0xFF);
}

TEST_CASE(EchoRelayedToTheScene) {
    Town t;
    json echo = { { "t", "echo" },          { "scene", kScene },          { "room", 0 },
                  { "id", 0x38 },           { "params", -1 },             { "pos", Arr(1.0, 2.0, 3.0) },
                  { "rot", Arr(0.0, 16384.0, 0.0) } };
    t.b->Send(echo); // neither owner nor lessee: dropped
    CHECK(!t.c->WaitFor("echo", t.s, 150).has_value());
    t.a->Send(echo);
    auto got = t.c->WaitFor("echo", t.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "from"), (int64_t)t.a->id);
    CHECK(t.b->WaitFor("echo", t.s).has_value());
    echo["id"] = 0x7FFF; // no such actor
    t.a->Send(echo);
    CHECK(!t.c->WaitFor("echo", t.s, 150).has_value());
}

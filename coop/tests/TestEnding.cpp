// The end of the game together: stages, the rooftop's countdown, the ending's sync, the cinema camera, the new cycle.
#include "TestWorld.h"

using namespace coop;
using namespace coop_test;

namespace {

constexpr int16_t kRooftop = 0x2C;
constexpr int16_t kOther = 0x10;

struct Players {
    TestServer s;
    std::unique_ptr<TestClient> a, b;
    explicit Players(server::ServerConfig cfg = {}) : s(cfg) {
        a = Join(s, "Alice");
        b = Join(s, "Bob");
        CreateWorld(s, *a);
        EnterWorld(s, *b);
        a->SendState(kRooftop, 0);
        b->SendState(kRooftop, 0);
        s.PumpFor(60);
    }
};

json Stage(int stage) {
    return { { "t", "ending" }, { "stage", stage } };
}

json Rooftop(int64_t ms) {
    return { { "t", "rooftop" }, { "ms", ms } };
}

// The latest "rooftop" (older ones dropped).
std::optional<json> LastRooftop(TestServer& s, TestClient& c, int timeoutMs = 300) {
    std::optional<json> last = c.WaitFor("rooftop", s, timeoutMs);
    while (auto ev = c.TakeEvent("rooftop")) {
        last = ev;
    }
    return last;
}

} // namespace

TEST_CASE(EndingStageTakesEveryoneInTheWorld) {
    Players p;
    p.b->SendState(kOther, 0); // anywhere in the world
    p.s.PumpFor(40);
    p.a->Send(Stage(3));
    auto got = p.b->WaitFor("ending", p.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "stage"), (int64_t)3);
    CHECK_EQ(GetString(*got, "nick"), std::string("Alice"));
    CHECK_EQ(GetInt(*got, "from"), (int64_t)p.a->id);
    CHECK(!p.a->WaitFor("ending", p.s, 150).has_value()); // not back to the sender
    p.a->Send(Stage(9)); // invalid
    CHECK(!p.b->WaitFor("ending", p.s, 150).has_value());
}

TEST_CASE(EndingForAllOffTakesNobody) {
    server::ServerConfig cfg;
    cfg.endingForAll = false;
    Players p(cfg);
    p.a->Send(Stage(1));
    CHECK(!p.b->WaitFor("ending", p.s, 200).has_value());
}

TEST_CASE(RooftopCountdownIsTheFirstOne) {
    Players p;
    p.a->Send(Rooftop(5000));
    auto a = LastRooftop(p.s, *p.a);
    auto b = LastRooftop(p.s, *p.b);
    CHECK(a.has_value() && b.has_value());
    CHECK_EQ(GetInt(*b, "ms"), (int64_t)5000);
    p.b->Send(Rooftop(300000)); // Bob's game started its own: it gets what is left of Alice's
    auto again = LastRooftop(p.s, *p.b);
    CHECK(again.has_value());
    CHECK(GetInt(*again, "ms") <= 5000 && GetInt(*again, "ms") > 4000);
    p.a->Send(Stage(2)); // Oath to Order: the giants hold the moon
    auto stopped = LastRooftop(p.s, *p.b);
    CHECK(stopped.has_value());
    CHECK_EQ(GetInt(*stopped, "ms"), (int64_t)-1);
}

TEST_CASE(RooftopCountdownRunsOutAndTheMoonFalls) {
    Players p;
    p.a->Send(Rooftop(300));
    CHECK(LastRooftop(p.s, *p.b).has_value());
    auto full = p.b->WaitFor("world_full", p.s, 1500);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("moon"));
    auto last = LastRooftop(p.s, *p.a, 100);
    CHECK(last.has_value());
    CHECK_EQ(GetInt(*last, "ms"), (int64_t)-1);
}

TEST_CASE(RooftopCountdownRejectsNonsense) {
    Players p;
    p.a->Send(Rooftop(0));
    p.a->Send(Rooftop(kRooftopMaxMs + 1));
    CHECK(!p.b->WaitFor("rooftop", p.s, 200).has_value());
}

TEST_CASE(EndingSyncReachesTheOthers) {
    Players p;
    json sync = { { "t", "ending_sync" }, { "seg", 2 }, { "scene", 0x2D }, { "cs", 0xFFF7 }, { "frame", 120 } };
    p.a->Send(sync);
    auto got = p.b->WaitFor("ending_sync", p.s);
    CHECK(got.has_value());
    CHECK_EQ(GetInt(*got, "frame"), (int64_t)120);
    CHECK_EQ(GetInt(*got, "from"), (int64_t)p.a->id);
    sync["frame"] = 70000; // out of range
    p.a->Send(sync);
    CHECK(!p.b->WaitFor("ending_sync", p.s, 150).has_value());
}

TEST_CASE(CinemaCameraOnlyToTheScene) {
    Players p;
    auto c = Join(p.s, "Carol");
    EnterWorld(p.s, *c);
    c->SendState(kOther, 0);
    p.s.PumpFor(60);
    json cam = { { "t", "cinema" }, { "eye", Arr(1.0, 2.0, 3.0) }, { "at", Arr(4.0, 5.0, 6.0) }, { "fov", 60.0 },
                 { "fill", Arr(255, 255, 255, 120) }, { "scope", "scene" } }; // Majora's lair: a boss arena
    p.a->Send(cam);
    CHECK(p.b->WaitFor("cinema", p.s).has_value());
    CHECK(!c->WaitFor("cinema", p.s, 150).has_value());
    cam["fov"] = 500.0;
    p.a->Send(cam);
    CHECK(!p.b->WaitFor("cinema", p.s, 150).has_value());
}

TEST_CASE(EndingDoneStartsANewCycle) {
    Players p;
    p.b->Send({ { "t", "ending_done" } });
    CHECK(p.b->WaitFor("cycle_compute", p.s).has_value()); // the game that finished computes it
    CHECK(!p.a->TakeEvent("cycle_compute").has_value());
}

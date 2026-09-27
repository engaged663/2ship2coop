// Clock and cycle rules on the server: /settime, Double Time, Inverted Song, the Song of Time vote, resets
// computed by a game, the moon, and the world commands.
#include "TestWorld.h"

#include "common/Clock.h"

#include <chrono>

using namespace coop;
using namespace coop_test;

namespace {

void SendResult(TestClient& c, uint8_t fill) {
    c.Send({ { "t", "cycle_result" }, { "fields", WorldFields(fill) } });
}

bool IsJump(const json& ev) {
    return GetBool(ev, "jump");
}

} // namespace

TEST_CASE(SetTimeJumpsEveryoneInTheWorld) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    s.server->ExecuteConsoleLine("settime 1 17:59");
    uint32_t target = 0;
    CHECK(clock::Parse(1, "17:59", target));
    for (TestClient* c : { a.get(), b.get() }) {
        auto jump = WaitClock(s, *c, IsJump);
        CHECK(jump.has_value());
        CHECK(GetInt(*jump, "abs") >= (int64_t)target && GetInt(*jump, "abs") < (int64_t)target + 60);
    }
    b->Cmd("/settime 2 10:00"); // admins only
    CHECK(b->WaitForSys("error", s));
    s.server->ExecuteConsoleLine("settime 4 10:00");
    CHECK(s.log.Lines().back().find("Uso: /settime") != std::string::npos);
}

TEST_CASE(DoubleTimeJumpsToTheNextHalfDay) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    a->Send({ { "t", "clock_jump" } });
    auto jump = WaitClock(s, *b, IsJump);
    CHECK(jump.has_value());
    CHECK(GetInt(*jump, "abs") >= clock::kHalfDayUnits && GetInt(*jump, "abs") < clock::kHalfDayUnits + 60);
    // On the final night it cannot go any further: the moon is the server's call.
    s.server->ExecuteConsoleLine("settime 3 20:00");
    Drain(s, { a.get(), b.get() });
    a->Send({ { "t", "clock_jump" } });
    CHECK(a->WaitForSys("warn", s));
    CHECK(!WaitClock(s, *b, IsJump, 300).has_value());
    CHECK(s.server->World().ClockAbs() < clock::kMoonAbs - 1000);
}

TEST_CASE(InvertedSongSlowsTimeForEveryone) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    a->Send({ { "t", "clock_speed" }, { "inv", true } });
    CHECK(WaitClock(s, *b, [](const json& ev) { return GetBool(ev, "inv"); }).has_value());
    uint32_t start = s.server->World().ClockAbs();
    auto t0 = std::chrono::steady_clock::now();
    s.PumpFor(1000);
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    double units = (double)(s.server->World().ClockAbs() - start);
    CHECK(units >= 20.0 * seconds - 3.0 && units <= 20.0 * seconds + 3.0);
    CHECK(s.server->World().TimeText().find("ralentizado") != std::string::npos);
    Drain(s, { a.get() }); // clocks sent before the change (the one right after creation says inv:false too)
    b->Send({ { "t", "clock_speed" }, { "inv", false } });
    CHECK(WaitClock(s, *a, [](const json& ev) { return !GetBool(ev, "inv"); }).has_value());
    CHECK(s.server->World().TimeText().find("velocidad normal") != std::string::npos);
}

TEST_CASE(SongOfTimeVotePassesWithMajority) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Cid");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    EnterWorld(s, *c);
    Drain(s, { a.get(), b.get(), c.get() });
    a->Send({ { "t", "sot_propose" } });
    auto asked = b->WaitFor("sys", s);
    CHECK(asked.has_value());
    CHECK(GetString(*asked, "text").find("/si") != std::string::npos);
    b->Cmd("/si"); // 2 of 3: passes
    CHECK(a->WaitFor("cycle_compute", s).has_value()); // the proposer's game computes it
    CHECK(!b->TakeEvent("cycle_compute").has_value());
    SendResult(*a, 9);
    for (TestClient* p : { a.get(), b.get(), c.get() }) {
        auto full = p->WaitFor("world_full", s);
        CHECK(full.has_value());
        CHECK_EQ(GetString(*full, "reset"), std::string("sot"));
        CHECK_EQ(GetInt(*full, "cycle"), 2);
        CHECK_EQ((*full)["fields"]["owls"].get<std::string>(), std::string("0909"));
        CHECK(GetInt((*full)["clock"], "abs") < 60);
    }
    CHECK_EQ(s.server->World().Cycle(), 2);
}

TEST_CASE(SongOfTimeVoteFailsOrTimesOut) {
    server::ServerConfig cfg;
    cfg.voteTimeoutMs = 300;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Cid");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    EnterWorld(s, *c);
    Drain(s, { a.get(), b.get(), c.get() });
    a->Send({ { "t", "sot_propose" } });
    s.PumpFor(50);
    b->Cmd("/no");
    c->Cmd("/no");
    CHECK(a->WaitForSys("warn", s)); // "no ha salido adelante"
    CHECK(!a->WaitFor("cycle_compute", s, 200).has_value());
    a->Send({ { "t", "sot_propose" } }); // nobody answers: time runs out
    CHECK(a->WaitForSys("warn", s, 1500));
    CHECK(!a->TakeEvent("cycle_compute").has_value());
    CHECK_EQ(s.server->World().Cycle(), 1);
    b->Cmd("/si"); // no vote any more
    CHECK(b->WaitForSys("warn", s));
}

TEST_CASE(SoloSongOfTimeResetsWithoutVote) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob"); // connected, not in the world: does not vote nor reset
    CreateWorld(s, *a);
    a->Send({ { "t", "sot_propose" } });
    CHECK(a->WaitFor("cycle_compute", s).has_value());
    SendResult(*a, 1);
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("sot"));
    CHECK_EQ(GetInt(*full, "cycle"), 2);
    CHECK(!b->WaitFor("world_full", s, 200).has_value());
}

TEST_CASE(RestartCommandResetsEveryone) {
    TestServer s;
    auto a = Join(s, "Alice");
    s.server->ExecuteConsoleLine("reiniciar"); // nobody in the world yet
    CHECK(s.log.Lines().back().find("Todavía no hay mundo") != std::string::npos);
    CreateWorld(s, *a);
    auto b = Join(s, "Bob");
    EnterWorld(s, *b);
    b->Cmd("/reiniciar"); // admins only
    CHECK(b->WaitForSys("error", s));
    s.server->ExecuteConsoleLine("reiniciar");
    CHECK(a->WaitFor("cycle_compute", s).has_value()); // the first player in the world
    b->Send({ { "t", "world_enter" } }); // already inside: ignored
    SendResult(*a, 4);
    for (TestClient* p : { a.get(), b.get() }) {
        auto full = p->WaitFor("world_full", s);
        CHECK(full.has_value());
        CHECK_EQ(GetString(*full, "reset"), std::string("sot"));
    }
}

TEST_CASE(CycleComputeMovesOnWhenAGameDoesNotAnswer) {
    server::ServerConfig cfg;
    cfg.cycleComputeTimeoutMs = 200;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    s.server->ExecuteConsoleLine("reiniciar");
    CHECK(a->WaitFor("cycle_compute", s).has_value());        // Alice never answers
    CHECK(b->WaitFor("cycle_compute", s, 1500).has_value());  // so Bob is asked
    SendResult(*a, 1); // too late: not hers any more
    SendResult(*b, 2);
    auto full = a->WaitFor("world_full", s);
    CHECK(full.has_value());
    CHECK_EQ((*full)["fields"]["owls"].get<std::string>(), std::string("0202"));
    s.PumpFor(100);
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 1u);
    // Nobody answers at all: the reset is called off and the clock runs again.
    Drain(s, { a.get(), b.get() });
    s.server->ExecuteConsoleLine("reiniciar");
    CHECK(a->WaitForSys("error", s, 2000));
    CHECK_EQ(s.server->World().Cycle(), 2);
    CHECK(WaitClock(s, *a, [](const json& ev) { return !GetBool(ev, "stopped"); }).has_value());
}

TEST_CASE(MoonRestoresTheCycleStart) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    json aStart = { { "r", 1 } };
    json bStart = { { "r", 2 } };
    a->Send({ { "t", "inv" }, { "inv", aStart }, { "cycle", 1 } });
    b->Send({ { "t", "inv" }, { "inv", bStart }, { "cycle", 1 } });
    s.PumpFor(100);
    a->Send({ { "t", "inv" }, { "inv", { { "r", 50 } } }, { "cycle", 1 } });
    b->Send({ { "t", "inv" }, { "inv", { { "r", 60 } } }, { "cycle", 1 } });
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(Field("weekEventReg"), 1, 0x01, 0) }) } });
    s.PumpFor(200);
    b->Close(); // Bob is away when the moon falls
    CHECK(a->WaitUntil(s, 3000, [&] { return s.server->Players().ByNick("Bob") == nullptr; }));
    s.server->ExecuteConsoleLine("settime 3 05:59");
    auto full = a->WaitFor("world_full", s, 3000);
    CHECK(full.has_value());
    CHECK_EQ(GetString(*full, "reset"), std::string("moon"));
    CHECK_EQ(GetInt(*full, "cycle"), 2);
    CHECK_EQ((*full)["you"]["inv"], aStart);
    std::vector<uint8_t> week;
    CHECK(FromHex((*full)["fields"]["weekEventReg"].get<std::string>(), week));
    CHECK_EQ(week[1], (uint8_t)0);
    CHECK(GetInt((*full)["clock"], "abs") < 60);
    auto bob = Join(s, "Bob");
    json bFull = EnterWorld(s, *bob);
    CHECK_EQ(bFull["you"]["inv"], bStart); // his cycle-start copy too
    CHECK(!GetBool(bFull["you"], "stale"));
}

TEST_CASE(PlayersWhoMissedTheSongOfTimeAreStale) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    b->Send({ { "t", "inv" }, { "inv", { { "r", 5 } } }, { "cycle", 1 } });
    s.PumpFor(100);
    b->Send({ { "t", "world_leave" } });
    s.PumpFor(100);
    a->Send({ { "t", "sot_propose" } }); // alone in the world now
    CHECK(a->WaitFor("cycle_compute", s).has_value());
    SendResult(*a, 0);
    CHECK(a->WaitFor("world_full", s).has_value());
    json full = EnterWorld(s, *b);
    CHECK(GetBool(full["you"], "stale")); // his game applies the end-of-cycle rules on entering
    CHECK_EQ(full["you"]["inv"], (json{ { "r", 5 } }));
    CHECK_EQ(GetInt(full, "cycle"), 2);
    b->Send({ { "t", "inv" }, { "inv", { { "r", 0 } } }, { "cycle", 2 } });
    s.PumpFor(100);
    b->Send({ { "t", "world_leave" } });
    s.PumpFor(100);
    CHECK(!GetBool(EnterWorld(s, *b)["you"], "stale"));
}

TEST_CASE(TimeAndWorldCommands) {
    TestServer s;
    auto a = Join(s, "Alice");
    a->Cmd("/tiempo");
    auto before = a->WaitFor("sys", s);
    CHECK(before.has_value());
    CHECK(GetString(*before, "text").find("Todavía no hay mundo") != std::string::npos);
    CreateWorld(s, *a);
    Drain(s, { a.get() });
    a->Cmd("/tiempo");
    auto now = a->WaitFor("sys", s);
    CHECK(now.has_value());
    std::string text = GetString(*now, "text");
    CHECK(text.find("Día 1, 06:0") != std::string::npos);
    CHECK(text.find("ciclo 1") != std::string::npos);
    a->Cmd("/mundo"); // admins only
    CHECK(a->WaitForSys("error", s));
    s.server->ExecuteConsoleLine("mundo");
    CHECK(s.log.Lines().back().find("Alice") != std::string::npos);
    s.server->ExecuteConsoleLine("si");
    CHECK(s.log.Lines().back().find("Solo votan") != std::string::npos);
}

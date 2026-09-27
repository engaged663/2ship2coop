// The shared world over the network: creating it, entering late, relaying changes, inventories, saving.
#include "TestWorld.h"

#include "common/Clock.h"

#include <fstream>

using namespace coop;
using namespace coop_test;

TEST_CASE(FirstPlayerCreatesTheWorldOthersWait) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    json full = EnterWorld(s, *a);
    CHECK(GetBool(full, "create"));
    CHECK_EQ(GetInt(full, "cycle"), 1);
    CHECK(full["you"]["inv"].is_null());
    b->Send({ { "t", "world_enter" } });
    CHECK(b->WaitForSys("info", s)); // "Se está creando el mundo..."
    CHECK(!b->TakeEvent("world_full").has_value());
    a->Send({ { "t", "world_init" }, { "fields", WorldFields(7) } });
    auto late = b->WaitFor("world_full", s);
    CHECK(late.has_value());
    CHECK(!GetBool(*late, "create"));
    CHECK_EQ(GetString(*late, "reset"), std::string(""));
    CHECK_EQ((*late)["fields"]["codes"].get<std::string>(), ToHex(std::vector<uint8_t>(20, 7)));
    CHECK((*late)["you"]["inv"].is_null());
    CHECK(!GetBool((*late)["you"], "stale"));
    CHECK(s.server->World().Exists());
    CHECK(s.server->Players().ByNick("Alice")->inWorld);
    CHECK(s.server->Players().ByNick("Bob")->inWorld);
}

TEST_CASE(CreatorTimeoutPassesToTheNextPlayer) {
    server::ServerConfig cfg;
    cfg.worldCreateTimeoutMs = 200;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CHECK(GetBool(EnterWorld(s, *a), "create"));
    b->Send({ { "t", "world_enter" } });
    auto full = b->WaitFor("world_full", s, 2000); // Alice never answers
    CHECK(full.has_value());
    CHECK(GetBool(*full, "create"));
    CHECK(a->WaitForSys("warn", s));
    a->Send({ { "t", "world_init" }, { "fields", WorldFields() } }); // too late: no longer hers
    b->Send({ { "t", "world_init" }, { "fields", WorldFields() } });
    CHECK(b->WaitUntil(s, 1000, [&] { return s.server->World().Exists(); }));
    s.PumpFor(100);
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 1u);
    CHECK(!s.server->Players().ByNick("Alice")->inWorld);
    CHECK(s.server->Players().ByNick("Bob")->inWorld);
}

TEST_CASE(WorldInitIsValidated) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    b->Send({ { "t", "world_init" }, { "fields", WorldFields() } }); // nobody asked Bob
    CHECK(GetBool(EnterWorld(s, *a), "create"));
    json broken = WorldFields();
    broken["sceneFlags"] = "00";
    a->Send({ { "t", "world_init" }, { "fields", broken } });
    a->Send({ { "t", "world_init" } });
    s.PumpFor(200);
    CHECK(!s.server->World().Exists());
    CHECK_EQ(s.server->Players().ByNick("Bob")->invalidMessages, 1u);
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 2u);
    a->Send({ { "t", "world_init" }, { "fields", WorldFields() } }); // still her turn: a valid one works
    CHECK(a->WaitUntil(s, 1000, [&] { return s.server->World().Exists(); }));
}

TEST_CASE(WorldOpsRelayedOnlyToPlayersInTheWorld) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    auto c = Join(s, "Cid"); // connected, but not playing in the server's world
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    a->Send({ { "t", "wops" },
              { "cycle", 1 },
              { "bits", json::array({ Arr(Field("weekEventReg"), 12, 0x20, 0) }) },
              { "bytes", json::array({ Arr(Field("masks"), 0, 0x3A) }) } });
    auto relayed = b->WaitFor("wops", s);
    CHECK(relayed.has_value());
    CHECK_EQ(GetInt(*relayed, "from"), (int64_t)a->id);
    CHECK_EQ((*relayed)["bits"][0], Arr(Field("weekEventReg"), 12, 0x20, 0));
    CHECK_EQ((*relayed)["bytes"][0], Arr(Field("masks"), 0, 0x3A));
    CHECK(!c->WaitFor("wops", s, 200).has_value());
    CHECK(!a->TakeEvent("wops").has_value()); // never echoed to the sender
    a->Send({ { "t", "wops" }, { "cycle", 0 }, { "bits", json::array({ Arr(Field("weekEventReg"), 13, 0x01, 0) }) } });
    CHECK(!b->WaitFor("wops", s, 200).has_value()); // from an older cycle: dropped, not invalid
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 0u);
    json full = EnterWorld(s, *c); // a late joiner gets it with the world
    std::vector<uint8_t> week;
    CHECK(FromHex(full["fields"]["weekEventReg"].get<std::string>(), week));
    CHECK_EQ(week[12], (uint8_t)0x20);
}

TEST_CASE(CountersAreClampedAndTheSenderCorrected) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    CreateWorld(s, *a);
    EnterWorld(s, *b);
    Drain(s, { a.get(), b.get() });
    uint16_t fairies = Field("fairies");
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "adds", json::array({ Arr(fairies, 1, 200) }) } });
    b->Send({ { "t", "wops" }, { "cycle", 1 }, { "adds", json::array({ Arr(fairies, 1, 100) }) } });
    // Whatever order they arrive in, the world ends at 255 and both games converge to it.
    int local[2] = { 200, 100 };
    TestClient* games[2] = { a.get(), b.get() };
    CHECK(a->WaitUntil(s, 2000, [&] {
        for (int i = 0; i < 2; i++) {
            while (auto ev = games[i]->TakeEvent("wops")) {
                for (const json& add : (*ev)["adds"]) {
                    local[i] += add[2].get<int>();
                }
            }
        }
        return local[0] == 255 && local[1] == 255;
    }));
    CHECK_EQ(s.server->World().Store().Fields()[fairies][1], (uint8_t)255);
}

TEST_CASE(InvalidWorldOpsCountAsInvalidPackets) {
    TestServer s;
    auto a = Join(s, "Alice");
    auto b = Join(s, "Bob");
    b->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(0, 0, 1, 0) }) } }); // not in the world: ignored
    CreateWorld(s, *a);
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", "x" } });                                         // malformed
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(999, 0, 1, 0) }) } });          // unknown field
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "adds", json::array({ Arr(Field("skulls"), 1, 1) }) } }); // misaligned
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bytes", json::array({ Arr(Field("owls"), 0, 1) }) } });  // wrong kind
    s.PumpFor(300);
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 4u);
    CHECK_EQ(s.server->Players().ByNick("Bob")->invalidMessages, 0u);
    for (int i = 0; i < kWopsBurst + 30; i++) { // floods are cut too
        a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(0, 1, 1, 0) }) } });
    }
    s.PumpFor(300);
    CHECK(s.server->Players().ByNick("Alice")->invalidMessages >= 4u + 10u);
}

TEST_CASE(InventoryAndPositionComeBackAfterReconnecting) {
    TestServer s;
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    json inv = { { "v", 1 },
                 { "fields", { { "rupees", "6300" } } },
                 { "loc", { { "entrance", 0xD800 }, { "room", 0 }, { "pos", Arr(1.5, 2, 3) }, { "rot", 16384 } } } };
    a->Send({ { "t", "inv" }, { "inv", inv }, { "cycle", 1 } });
    a->Send({ { "t", "inv" }, { "inv", { { "v", 1 } } }, { "cycle", 7 } }); // from another cycle: ignored
    s.PumpFor(200);
    a->Close();
    CHECK(a->WaitUntil(s, 3000, [&] { return s.server->Players().ByNick("Alice") == nullptr; }));
    auto again = Join(s, "Alice");
    json full = EnterWorld(s, *again);
    CHECK_EQ(full["you"]["inv"], inv);
    CHECK(!GetBool(full["you"], "stale"));
}

TEST_CASE(OversizedOrBrokenInventoryRejected) {
    TestServer s;
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    a->Send({ { "t", "inv" }, { "inv", { { "pad", std::string(kMaxInventoryBytes, 'a') } } }, { "cycle", 1 } });
    a->Send({ { "t", "inv" }, { "inv", "texto" }, { "cycle", 1 } });
    a->Send({ { "t", "inv" }, { "cycle", 1 } });
    s.PumpFor(200);
    CHECK_EQ(s.server->Players().ByNick("Alice")->invalidMessages, 3u);
    // The biggest valid inventory still fits in the world_full the server sends back (one ENet packet).
    json fat = { { "pad", std::string(kMaxInventoryBytes - 20, 'b') } };
    CHECK(SerializeEvent(fat).size() <= kMaxInventoryBytes);
    a->Send({ { "t", "inv" }, { "inv", fat }, { "cycle", 1 } });
    s.PumpFor(100);
    a->Send({ { "t", "world_leave" } });
    s.PumpFor(100);
    json full = EnterWorld(s, *a);
    CHECK_EQ(full["you"]["inv"], fat);
}

TEST_CASE(WorldSurvivesServerRestart) {
    TempDir dir("coop_test_world_restart");
    server::ServerConfig cfg;
    cfg.worldPath = dir.File("world.json");
    cfg.playersDir = dir.File("players");
    json inv = { { "v", 1 }, { "rupees", 42 } };
    uint32_t noon = 0;
    CHECK(clock::Parse(2, "12:00", noon));
    {
        TestServer s(cfg);
        auto a = Join(s, "Alice");
        CreateWorld(s, *a, 3);
        a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr(Field("weekEventReg"), 5, 0x10, 0) }) } });
        a->Send({ { "t", "inv" }, { "inv", inv }, { "cycle", 1 } });
        std::string err;
        CHECK(s.server->World().SetTime(noon, "prueba", &err));
        s.PumpFor(200);
        s.server->Stop("prueba");
        CHECK(a->WaitUntil(s, 3000, [&] { return !s.server->IsRunning(); }));
    }
    CHECK(std::filesystem::exists(cfg.worldPath));
    {
        TestServer s(cfg);
        CHECK(s.server->World().Exists());
        auto a = Join(s, "Alice");
        json full = EnterWorld(s, *a);
        std::vector<uint8_t> week;
        CHECK(FromHex(full["fields"]["weekEventReg"].get<std::string>(), week));
        CHECK_EQ(week[5], (uint8_t)(0x03 | 0x10));
        CHECK_EQ(full["you"]["inv"], inv);
        int64_t abs = GetInt(full["clock"], "abs");
        CHECK(abs >= (int64_t)noon && abs < (int64_t)noon + 600); // it only ran while Alice was inside
    }
    // A damaged world.json is set aside (never overwritten) and the next player creates a new world.
    {
        std::ofstream broken(cfg.worldPath);
        broken << "{roto";
    }
    TestServer s(cfg);
    CHECK(!s.server->World().Exists());
    CHECK(std::filesystem::exists(cfg.worldPath + ".bad"));
}

TEST_CASE(ClockRunsOnlyWithSomeoneInTheWorld) {
    server::ServerConfig cfg;
    cfg.clockBroadcastMs = 50;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    CHECK(WaitClock(s, *a, [](const json& c) { return !GetBool(c, "stopped") && GetInt(c, "abs") > 0; }));
    // A scene where the original game stops time (the Moon) stops it for everyone.
    a->Send({ { "t", "loc" }, { "scene", 0x0B }, { "room", 0 }, { "entrance", 0 }, { "sceneName", "Luna" },
              { "timeStopped", true } });
    CHECK(WaitClock(s, *a, [](const json& c) { return GetBool(c, "stopped"); }));
    CHECK(s.server->World().TimeText().find("Alice") != std::string::npos);
    a->Send({ { "t", "loc" }, { "scene", 0x0B }, { "room", 0 }, { "entrance", 0 }, { "sceneName", "Luna" },
              { "timeStopped", false } });
    CHECK(WaitClock(s, *a, [](const json& c) { return !GetBool(c, "stopped"); }));
    a->Send({ { "t", "world_leave" } });
    s.PumpFor(100);
    uint32_t before = s.server->World().ClockAbs();
    s.PumpFor(300);
    CHECK_EQ(s.server->World().ClockAbs(), before); // nobody inside: stopped
    CHECK(s.server->World().TimeText().find("nadie") != std::string::npos);
}

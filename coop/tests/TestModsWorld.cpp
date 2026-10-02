// The shared world seen by mods: the clock's speed, world.* and the world's events.
#include "TestMods.h"

#include "common/Clock.h"
#include "server/World/WorldClock.h"

using namespace coop;
using namespace coop_test;

TEST_CASE(WorldClockSpeedScalesTime) {
    server::WorldClock c;
    c.Set(0, 0);
    c.SetRunning(true, 0);
    CHECK_EQ(c.Abs(1000), (uint32_t)60);
    c.SetScale(2.0, 1000); // what already passed stays; from now on twice as fast
    CHECK_EQ(c.Abs(2000), (uint32_t)180);
    CHECK_EQ(c.UnitsPerSecond(), 120.0);
    c.SetInverted(true, 2000);
    CHECK_EQ(c.Abs(3000), (uint32_t)220); // 20 * 2
    c.SetScale(0.5, 3000);
    CHECK_EQ(c.Abs(5000), (uint32_t)240);
    c.SetScale(1000.0, 5000); // out of range: the fastest allowed
    CHECK_EQ(c.Scale(), kMaxTimeSpeed);
    c.SetRunning(false, 5000);
    CHECK_EQ(c.UnitsPerSecond(), 0.0);
}

TEST_CASE(TimeSpeedFromTheConfigReachesTheGames) {
    server::ServerConfig cfg;
    cfg.timeSpeed = 2.0;
    cfg.clockBroadcastMs = 50;
    TestServer s(cfg);
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    auto clock = WaitClock(s, *a, [](const json& ev) { return !GetBool(ev, "stopped"); });
    CHECK(clock.has_value());
    CHECK_EQ(GetNumber(*clock, "ups"), 120.0);
    s.server->ExecuteConsoleLine("tiempo");
    CHECK(Logged(s, "x2")); // /tiempo says the speed when it is not the normal one
}

TEST_CASE(ModsReadAndMoveTheClock) {
    ModServer m("coop_world_clock");
    auto a = Join(m.s, "Alice");
    CHECK_EQ(m.Load("antes", "print('existe=' .. tostring(coop.world.exists()) .. ' hora=' .. "
                             "tostring(coop.world.time()) .. ' ciclo=' .. coop.world.cycle())"),
             std::string());
    CHECK(m.Logged("existe=false hora=nil ciclo=0"));
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    CHECK_EQ(m.Load("reloj", R"lua(
        coop.world.setTime(2, 18, 30)
        local t = coop.world.time()
        print("dia=" .. t.day .. " hora=" .. t.hour .. ":" .. t.minute .. " noche=" .. tostring(t.night) ..
              " ciclo=" .. coop.world.cycle())
        coop.world.setStopped(true)
        coop.world.setStopped(true)
        print("parado=" .. tostring(coop.world.time().stopped))
        coop.world.setSpeed(0.5)
        print("vel=" .. coop.world.time().speed .. " info=" .. coop.server.info().timeSpeed)
        print("malo=" .. tostring(pcall(coop.world.setTime, 4, 0)) .. tostring(pcall(coop.world.setSpeed, 50)))
    )lua"),
             std::string());
    CHECK(m.Logged("dia=2 hora=18:30 noche=true ciclo=1"));
    CHECK(m.Logged("parado=true"));
    CHECK(m.Logged("vel=0.5 info=0.5"));
    CHECK(m.Logged("malo=falsefalse"));
    auto jump = WaitClock(m.s, *a, [](const json& ev) { return GetBool(ev, "jump"); });
    CHECK(jump.has_value());
    CHECK_EQ(clock::DayOfAbs((uint32_t)GetInt(*jump, "abs")), 2);
    auto b = Join(m.s, "Bob"); // someone entering must not restart a clock a mod stopped
    EnterWorld(m.s, *b);
    m.s.PumpFor(100);
    CHECK(m.s.server->World().ClockStopped());
    Drain(m.s, { a.get(), b.get() });
    CHECK_EQ(m.Load("sigue", "coop.world.setStopped(false) print('corre=' .. tostring(not coop.world.time().stopped))"),
             std::string());
    CHECK(m.Logged("corre=true"));
    auto running = WaitClock(m.s, *a, [](const json& ev) { return !GetBool(ev, "stopped"); });
    CHECK(running.has_value());
    CHECK_EQ(GetNumber(*running, "ups"), 30.0);
}

TEST_CASE(ModsChangeTheSharedWorld) {
    ModServer m("coop_world_ops");
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    Drain(m.s, { a.get(), b.get() });
    CHECK_EQ(m.Load("mundo", R"lua(
        coop.on("world_change", function(e)
            print("cambio de " .. e.player .. " bits=" .. #e.bits .. " bytes=" .. #e.bytes .. " adds=" .. #e.adds)
        end)
        print("campos=" .. #coop.world.fields() .. " primero=" .. coop.world.fields()[1].name ..
              " " .. coop.world.fields()[1].kind .. " " .. coop.world.fields()[1].size)
        print("puesto=" .. tostring(coop.world.setBits("weekEventReg", 3, 0x12)))
        print("otra=" .. tostring(coop.world.setBits("weekEventReg", 3, 0x02)))
        print("bit=" .. tostring(coop.world.getBit("weekEventReg", 3, 0x10)) ..
              tostring(coop.world.getBit("weekEventReg", 3, 0x04)) .. " byte=" .. coop.world.get("weekEventReg", 3))
        print("quita=" .. tostring(coop.world.setBits("weekEventReg", 3, 0, 0x02)) .. " byte=" ..
              coop.world.get("weekEventReg", 3))
        print("mascara=" .. tostring(coop.world.set("masks", 2, 0x39)) .. " vale=" .. coop.world.get("masks", 2))
        print("hadas=" .. coop.world.add("fairies", 1, 300) .. " total=" .. coop.world.get("fairies", 1))
        print("contador=" .. tostring(coop.world.set("fairies", 2, 7)) .. " vale=" .. coop.world.get("fairies", 2))
        print("corazones=" .. coop.world.add("heartQuarters", 0, 12) .. " vale=" .. coop.world.get("heartQuarters", 0))
        print("errores=" .. tostring(pcall(coop.world.setBits, "items", 0, 1)) ..
              tostring(pcall(coop.world.set, "weekEventReg", 0, 1)) ..
              tostring(pcall(coop.world.get, "weekEventReg", 100)) ..
              tostring(pcall(coop.world.get, "noExiste", 0)) .. tostring(pcall(coop.world.add, "masks", 0, 1)) ..
              tostring(pcall(coop.world.get, "heartQuarters", 1)))
    )lua"),
             std::string());
    CHECK(m.Logged("campos=" + std::to_string(world::kFieldCount) + " primero=weekEventReg bits 100"));
    CHECK(m.Logged("puesto=true"));
    CHECK(m.Logged("otra=false")); // nothing changed: no event, nothing sent
    CHECK(m.Logged("bit=truefalse byte=18"));
    CHECK(m.Logged("quita=true byte=16"));
    CHECK(m.Logged("mascara=true vale=57"));
    CHECK(m.Logged("hadas=255 total=255")); // clamped to the counter's range
    CHECK(m.Logged("contador=true vale=7"));
    CHECK(m.Logged("corazones=12 vale=12")); // a two-byte counter
    CHECK(m.Logged("errores=falsefalsefalsefalsefalsefalse"));
    CHECK(m.Logged("cambio de 0 bits=1 bytes=0 adds=0"));
    CHECK(m.Logged("cambio de 0 bits=0 bytes=1 adds=0"));
    CHECK(m.Logged("cambio de 0 bits=0 bytes=0 adds=1"));
    auto wops = b->WaitFor("wops", m.s);
    CHECK(wops.has_value());
    CHECK_EQ(GetInt(*wops, "from", -1), (int64_t)0);
    CHECK((*wops)["bits"][0] == Arr((int)Field("weekEventReg"), 3, 0x12, 0));
    CHECK(a->WaitFor("wops", m.s).has_value()); // everyone, Alice too
    // What a game changes is heard as well.
    a->Send({ { "t", "wops" }, { "cycle", 1 }, { "bits", json::array({ Arr((int)Field("owls"), 0, 1, 0) }) } });
    m.s.PumpFor(100);
    CHECK(m.Logged("cambio de 1 bits=1 bytes=0 adds=0"));
}

TEST_CASE(WorldFunctionsNeedAWorld) {
    ModServer m("coop_world_none");
    CHECK_EQ(m.Load("vacio", R"lua(
        local function falla(f, ...) return tostring(pcall(f, ...)) end
        print("sin mundo=" .. falla(coop.world.get, "masks", 0) .. falla(coop.world.setBits, "owls", 0, 1) ..
              falla(coop.world.setTime, 1, 12) .. falla(coop.world.restart) .. falla(coop.world.crashMoon) ..
              falla(coop.world.setStopped, true))
        coop.world.setSpeed(3)
        print("vel=" .. coop.server.info().timeSpeed)
    )lua"),
             std::string());
    CHECK(m.Logged("sin mundo=falsefalsefalsefalsefalsefalse"));
    CHECK(m.Logged("vel=3")); // the speed of the world to come
}

TEST_CASE(ModsHearTheWorldFillAndTheCycleTurn) {
    ModServer m("coop_world_cycle");
    CHECK_EQ(m.Load("ciclo", R"lua(
        for _, name in ipairs({ "world_created", "world_enter", "world_leave" }) do
            coop.on(name, function(e) print(name .. " " .. e.nick) end)
        end
        coop.on("cycle_reset", function(e) print("ciclo " .. e.cycle .. " por " .. e.reason) end)
        coop.on("moon_crash", function(e) print("luna en el ciclo " .. e.cycle) end)
        coop.on("vote_end", function(e) print("voto " .. tostring(e.passed)) end)
        coop.on("vote_start", function(e)
            print("propone " .. e.nick)
            if coop.world.cycle() == 3 then return false end
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    m.s.PumpFor(50);
    CHECK(m.Logged("world_created Alice"));
    CHECK(m.Logged("world_enter Alice"));
    CHECK(m.Logged("world_enter Bob"));
    // A restart: the server asks a game to compute the new cycle.
    m.s.server->ExecuteConsoleLine("reiniciar");
    CHECK(a->WaitFor("cycle_compute", m.s).has_value());
    a->Send({ { "t", "cycle_result" }, { "fields", WorldFields(0) } });
    m.s.PumpFor(100);
    CHECK(m.Logged("ciclo 2 por restart"));
    // The moon.
    m.s.server->World().CrashMoon();
    CHECK(m.Logged("luna en el ciclo 2"));
    CHECK(m.Logged("ciclo 3 por moon"));
    // In cycle 3 the mod refuses the Song of Time: no vote starts.
    Drain(m.s, { a.get(), b.get() });
    a->Send({ { "t", "sot_propose" } });
    m.s.PumpFor(100);
    CHECK(m.Logged("propone Alice"));
    CHECK(!m.Logged("voto "));
    a->Cmd("/si");
    CHECK(a->WaitForSys("warn", m.s)); // "no vote in progress"
    b->Send({ { "t", "world_leave" } });
    m.s.PumpFor(100);
    CHECK(m.Logged("world_leave Bob"));
}

TEST_CASE(SongOfTimeVoteEventsFollowTheVote) {
    server::ServerConfig cfg;
    cfg.voteTimeoutMs = 300;
    ModServer m("coop_world_vote", cfg);
    CHECK_EQ(m.Load("voto", R"lua(
        coop.on("vote_end", function(e) print("voto " .. tostring(e.passed)) end)
        coop.on("cycle_reset", function(e) print("ciclo " .. e.cycle .. " por " .. e.reason) end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    a->Send({ { "t", "sot_propose" } });
    m.s.PumpFor(500); // nobody else votes: it fails
    CHECK(m.Logged("voto false"));
    a->Send({ { "t", "sot_propose" } });
    m.s.PumpFor(50);
    b->Cmd("/si");
    CHECK(a->WaitFor("cycle_compute", m.s).has_value());
    CHECK(m.Logged("voto true"));
    a->Send({ { "t", "cycle_result" }, { "fields", WorldFields(0) } });
    m.s.PumpFor(100);
    CHECK(m.Logged("ciclo 2 por sot"));
}

TEST_CASE(WorldHourEventFollowsTheClock) {
    server::ServerConfig cfg;
    cfg.timeSpeed = 10.0; // 600 units a second: a minute of the game takes 76 ms
    ModServer m("coop_world_hour", cfg);
    CHECK_EQ(m.Load("horas", R"lua(
        coop.on("world_hour", function(e)
            print("hora dia=" .. e.day .. " h=" .. e.hour .. " noche=" .. tostring(e.night) ..
                  " salto=" .. tostring(e.jump))
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    m.s.PumpFor(50);
    CHECK(!m.Logged("hora dia")); // the first hour seen is not news
    CHECK_EQ(m.Load("salta", "coop.world.setTime(1, 17, 55)"), std::string());
    m.s.PumpFor(50);
    CHECK(m.Logged("hora dia=1 h=17 noche=false salto=true"));
    m.s.PumpFor(700); // 18:00 arrives by itself
    CHECK(m.Logged("hora dia=1 h=18 noche=true salto=false"));
}

TEST_CASE(FreezeTimeSurvivesPlayersComingAndGoing) {
    TestServer s;
    auto a = Join(s, "Alice");
    CreateWorld(s, *a);
    s.server->ExecuteConsoleLine("freezetime");
    CHECK(s.server->World().ClockStopped());
    auto b = Join(s, "Bob");
    EnterWorld(s, *b);
    b->Send({ { "t", "world_leave" } });
    s.PumpFor(100);
    CHECK(s.server->World().ClockStopped()); // it used to start again here
    s.server->ExecuteConsoleLine("tiempo");
    s.server->ExecuteConsoleLine("freezetime on");
    CHECK(Logged(s, "ya está detenido"));
    s.server->ExecuteConsoleLine("freezetime");
    CHECK(!s.server->World().ClockStopped());
}

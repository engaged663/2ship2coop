// The bridge to the games, on the server: names of the game's things, orders, forced settings and what games report.
#include "TestMods.h"

#include "server/CommandRegistry.h"
#include "server/Mods/GameIds.h"

#include "common/Text.h"

using namespace coop;
using namespace coop_test;

namespace {

// The first order of the next "mod" event.
json NextOp(TestServer& s, TestClient& c) {
    auto ev = c.WaitFor("mod", s);
    if (!ev.has_value() || !(*ev)["ops"].is_array() || (*ev)["ops"].empty()) {
        Fail(__FILE__, __LINE__, "no order arrived");
    }
    return (*ev)["ops"][0];
}

} // namespace

TEST_CASE(GameIdsKnowItemsActorsAndScenes) {
    using ids::Kind;
    CHECK_EQ(ids::Find(Kind::Item, "MASK_BUNNY"), 0x39);
    CHECK_EQ(ids::Find(Kind::Item, "item_mask_bunny"), 0x39);
    CHECK_EQ(ids::Find(Kind::Item, 0x39), 0x39);
    CHECK_EQ(ids::Find(Kind::Item, 57.0), 0x39); // what a division leaves in Lua
    CHECK_EQ(ids::Find(Kind::Item, "NO_EXISTE"), -1);
    CHECK_EQ(ids::Find(Kind::Item, 0x1000), -1);
    CHECK_EQ(ids::Find(Kind::Item, -1), -1);
    CHECK_EQ(ids::Find(Kind::Item, 1.5), -1);
    CHECK_EQ(ids::Find(Kind::Item, json::array()), -1);
    CHECK_EQ(ids::NameOf(Kind::Item, 0x00), std::string("OCARINA_OF_TIME"));
    CHECK_EQ(ids::NameOf(Kind::Item, 0x1000), std::string());
    CHECK_EQ(ids::Find(Kind::Actor, "EN_DODONGO"), 0x0B);
    CHECK_EQ(ids::Find(Kind::Actor, "Actor_En_Dodongo"), 0x0B);
    CHECK_EQ(ids::TitleOf(Kind::Actor, 0x0B), std::string("Dodongo"));
    CHECK_EQ(ids::Find(Kind::Actor, "EN_COOP_PUPPET"), -1); // the co-op's own actor is nobody's to name
    CHECK_EQ(ids::Find(Kind::Scene, "SOUTH_CLOCK_TOWN"), 0x6F);
    CHECK_EQ(ids::Find(Kind::Scene, "clocktower"), 0x6F); // the decomp's own name works too
    CHECK_EQ(ids::Find(Kind::Scene, "SCENE_CLOCKTOWER"), 0x6F);
    CHECK_EQ(ids::Find(Kind::Scene, 0x01), -1); // a hole in the game's table
    CHECK_EQ(ids::NameOf(Kind::Scene, 0x6F), std::string("SOUTH_CLOCK_TOWN"));
    CHECK_EQ(ids::TitleOf(Kind::Scene, 0x6F), std::string("South Clock Town"));
    CHECK_EQ(ids::EntranceSceneOf(0x6F), 0x6C);
    CHECK_EQ(ids::EntranceSceneOf(0x01), -1);
    CHECK(ids::Table(Kind::Scene).size() > 90);
    CHECK(ids::Table(Kind::Actor).size() > 500);
    CHECK(ids::Table(Kind::Item)["BOW"] == 1);
    Kind kind = Kind::Item;
    CHECK(ids::ParseKind("Scene", kind) && kind == Kind::Scene);
    CHECK(ids::ParseKind("actor", kind) && kind == Kind::Actor);
    CHECK(!ids::ParseKind("cosa", kind));
}

TEST_CASE(GameOrdersReachOnlyTheirTargets) {
    ModServer m("coop_game_orders");
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    auto c = Join(m.s, "Cid"); // connected, but not playing in the server's world
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    Drain(m.s, { a.get(), b.get(), c.get() });
    CHECK_EQ(m.Load("ordenes", R"lua(
        print("uno=" .. coop.game.giveItem("Alice", "MASK_BUNNY"))
        print("todos=" .. coop.game.heal("*"))
        print("lista=" .. coop.game.giveRupees({ "Alice", 2 }, 50))
        print("fuera=" .. coop.game.kill("Cid"))
        print("nadie=" .. tostring(pcall(coop.game.kill, "Zed")))
    )lua"),
             std::string());
    CHECK(m.Logged("uno=1"));
    CHECK(m.Logged("todos=2"));
    CHECK(m.Logged("lista=2"));
    CHECK(m.Logged("fuera=0")); // Cid is not in the world: nothing is sent
    CHECK(m.Logged("nadie=false"));
    json item = NextOp(m.s, *a);
    CHECK_EQ(item["op"].get<std::string>(), std::string("item"));
    CHECK_EQ(item["id"].get<int>(), 0x39);
    CHECK_EQ(NextOp(m.s, *a)["op"].get<std::string>(), std::string("heal"));
    json rupees = NextOp(m.s, *a);
    CHECK_EQ(rupees["amount"].get<int>(), 50);
    CHECK_EQ(NextOp(m.s, *b)["op"].get<std::string>(), std::string("heal")); // Bob never got the mask
    CHECK(!c->WaitFor("mod", m.s, 200).has_value());
}

TEST_CASE(GameOrdersCheckTheirArguments) {
    ModServer m("coop_game_args");
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    CHECK_EQ(m.Load("malos", R"lua(
        local function falla(f, ...) return tostring(pcall(f, ...)) end
        print("a=" .. falla(coop.game.giveItem, "Alice", "ESPADA_LASER") .. falla(coop.game.giveItem, "Alice", 999))
        print("b=" .. falla(coop.game.damage, "Alice", 0) .. falla(coop.game.damage, "Alice", 99999))
        print("c=" .. falla(coop.game.giveRupees, "Alice", 0) .. falla(coop.game.sound, "Alice", -1))
        print("d=" .. falla(coop.game.spawn, "Alice", "NO_HAY") .. falla(coop.game.spawn, "Alice", "PLAYER"))
        print("e=" .. falla(coop.game.warp, "Alice", "NO_HAY") .. falla(coop.game.warp, "Alice", "SOUTH_CLOCK_TOWN", 99))
        print("f=" .. falla(coop.game.notify, "Alice", "") .. falla(coop.game.notify, "Alice", "x", 500))
        print("g=" .. falla(coop.game.giveItem, "Alice", "MAP_POINT_STONE_TOWER") ..
              falla(coop.game.spawn, "Alice", "EN_DODONGO", { distance = 99999 }))
        print("h=" .. falla(coop.game.spawn, "Alice", "EN_DODONGO", { x = 1 }) ..
              falla(coop.game.message, "Alice", " \n "))
    )lua"),
             std::string());
    for (const char* line : { "a=falsefalse", "b=falsefalse", "c=falsefalse", "d=falsefalse", "e=falsefalse",
                              "f=falsefalse", "g=falsefalse", "h=falsefalse" }) {
        CHECK(m.Logged(line));
    }
    CHECK(!a->WaitFor("mod", m.s, 200).has_value()); // nothing wrong ever leaves the server
}

TEST_CASE(GiveItemRefusesWhatTheGameCannotGive) {
    ModServer m("coop_game_givable");
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    CHECK_EQ(m.Load("dar", R"lua(
        for _, nombre in ipairs({ "SWORD_DEITY", "WALLET_DEFAULT", "FISHING_ROD", "STRAY_FAIRIES", "INVALID_1" }) do
            print("dar " .. nombre .. "=" .. tostring(pcall(coop.game.giveItem, "Alice", nombre)))
        end
        print("quitar=" .. tostring(pcall(coop.game.takeItem, "Alice", "INVALID_7")))
        print("bien=" .. coop.game.giveItem("Alice", "SWORD_GILDED"))
    )lua"),
             std::string());
    for (const char* line : { "dar SWORD_DEITY=false", "dar WALLET_DEFAULT=false", "dar FISHING_ROD=false",
                              "dar STRAY_FAIRIES=false", "dar INVALID_1=false", "quitar=false", "bien=1" }) {
        CHECK(m.Logged(line));
    }
    CHECK_EQ(NextOp(m.s, *a)["id"].get<int>(), 0x4F); // only the sword the game can give left the server
}

TEST_CASE(GameTextsAreCutAndCleaned) {
    ModServer m("coop_game_text");
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    CHECK_EQ(m.Load("texto", R"lua(
        coop.game.notify("Alice", string.rep("ñ", 10000), 3)
        coop.game.message("Alice", "linea 1\nlinea 2\tfin\0oculto \xFF")
    )lua"),
             std::string());
    json notify = NextOp(m.s, *a);
    std::string text = notify["text"].get<std::string>();
    CHECK_EQ(text.size(), (size_t)kMaxModText * 2); // 400 characters of two bytes each
    CHECK(IsValidUtf8(text));
    CHECK_EQ(notify["seconds"].get<int>(), 3);
    json message = NextOp(m.s, *a);
    std::string msg = message["text"].get<std::string>();
    CHECK(msg.find("linea 1\nlinea 2 fin") == 0); // line breaks stay, other control characters go
    CHECK(IsValidUtf8(msg));
}

TEST_CASE(GameSpawnAndWarpBuildTheirOrders) {
    ModServer m("coop_game_spawn");
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    Drain(m.s, { a.get() });
    CHECK_EQ(m.Load("crea", R"lua(
        coop.game.spawn("Alice", "EN_DODONGO")
        coop.game.spawn("Alice", 0x0C, { params = 3, x = 10, y = 20.5, z = -30, rotY = 16384 })
        coop.game.warp("Alice", "SOUTH_CLOCK_TOWN", 2)
        coop.game.warp("Alice", 0x2D)
        coop.game.unlockAll("Alice")
        coop.game.magic("Alice")
        coop.game.magic("Alice", -10)
        coop.game.takeItem("Alice", "BOW")
        coop.game.sound("Alice", 0x4803)
    )lua"),
             std::string());
    json first = NextOp(m.s, *a);
    CHECK_EQ(first["op"].get<std::string>(), std::string("spawn"));
    CHECK_EQ(first["actor"].get<int>(), 0x0B);
    CHECK_EQ(first["params"].get<int>(), 0);
    CHECK_EQ(first["dist"].get<int>(), 100);
    CHECK(!first.contains("pos")); // in front of Link
    json second = NextOp(m.s, *a);
    CHECK_EQ(second["params"].get<int>(), 3);
    CHECK_EQ(second["pos"][1].get<double>(), 20.5);
    CHECK_EQ(second["rotY"].get<int>(), 16384);
    json warp = NextOp(m.s, *a);
    CHECK_EQ(warp["entrance"].get<int>(), (0x6C << 9) | (2 << 4));
    CHECK_EQ(NextOp(m.s, *a)["entrance"].get<int>(), 0x2A << 9); // Termina Field, by scene id
    CHECK(a->WaitFor("unlock_all", m.s).has_value());
    CHECK_EQ(NextOp(m.s, *a)["amount"].get<int>(), 0);
    CHECK_EQ(NextOp(m.s, *a)["amount"].get<int>(), -10);
    json take = NextOp(m.s, *a);
    CHECK_EQ(take["op"].get<std::string>(), std::string("take"));
    CHECK_EQ(take["id"].get<int>(), 1);
    CHECK_EQ(NextOp(m.s, *a)["id"].get<int>(), 0x4803);
}

TEST_CASE(GameSettingsReachNewcomersAndDieWithTheConnection) {
    server::ServerConfig cfg;
    cfg.gameSettings = { { "gEnhancements.DifficultyOptions.DamageMultiplier", 1 } };
    ModServer m("coop_game_settings", cfg);
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    auto first = a->WaitFor("mod_cfg", m.s);
    CHECK(first.has_value()); // what server.json forces arrives on entering
    CHECK_EQ((*first)["settings"]["gEnhancements.DifficultyOptions.DamageMultiplier"].get<int>(), 1);
    CHECK_EQ(m.Load("opciones", R"lua(
        coop.game.setSetting("*", "gCheats.InfiniteMagic", 1)
        print("malo=" .. tostring(pcall(coop.game.setSetting, "*", "gSettings.Fullscreen", 1)) ..
              tostring(pcall(coop.game.setSetting, "*", "gCheats.InfiniteMagic", "si")))
        coop.commands.register("salto", { help = "x" }, function(ctx, args)
            coop.game.setSetting(args[1], "gCheats.MoonJumpOnL", 1)
            coop.game.setSetting(args[1], "gCheats.SpeedModifier.Value", 1.5)
        end)
        coop.commands.register("quita", { help = "x" }, function()
            coop.game.clearSetting("*", "gCheats.InfiniteMagic")
            local n = 0
            for _ in pairs(coop.game.settings()) do n = n + 1 end
            print("globales=" .. n)
        end)
    )lua"),
             std::string());
    CHECK(m.Logged("malo=falsefalse"));
    auto second = a->WaitFor("mod_cfg", m.s);
    CHECK(second.has_value());
    CHECK_EQ((*second)["settings"].size(), (size_t)2); // always the whole map
    auto b = Join(m.s, "Bob");
    EnterWorld(m.s, *b);
    auto late = b->WaitFor("mod_cfg", m.s);
    CHECK(late.has_value());
    CHECK_EQ((*late)["settings"]["gCheats.InfiniteMagic"].get<int>(), 1);
    Drain(m.s, { a.get(), b.get() });
    m.s.server->ExecuteConsoleLine("salto Bob");
    auto own = b->WaitFor("mod_cfg", m.s);
    CHECK(own.has_value());
    CHECK_EQ((*own)["settings"].value("gCheats.MoonJumpOnL", 0), 1); // both changes in one event
    CHECK_EQ((*own)["settings"].value("gCheats.SpeedModifier.Value", 0.0), 1.5);
    CHECK(!b->WaitFor("mod_cfg", m.s, 200).has_value());
    CHECK(!a->WaitFor("mod_cfg", m.s, 200).has_value()); // only Bob's
    b->events.clear();
    m.s.server->ExecuteConsoleLine("quita");
    CHECK(m.Logged("globales=1"));
    auto lessA = a->WaitFor("mod_cfg", m.s);
    auto lessB = b->WaitFor("mod_cfg", m.s);
    CHECK(lessA.has_value() && lessB.has_value());
    CHECK(!(*lessA)["settings"].contains("gCheats.InfiniteMagic"));
    CHECK((*lessB)["settings"].contains("gCheats.MoonJumpOnL"));
    // Bob leaves; whoever takes his id starts clean.
    b->Close();
    m.s.PumpFor(1500);
    auto c = Join(m.s, "Cid");
    CHECK_EQ(c->id, (uint8_t)2);
    EnterWorld(m.s, *c);
    auto fresh = c->WaitFor("mod_cfg", m.s);
    CHECK(fresh.has_value());
    CHECK(!(*fresh)["settings"].contains("gCheats.MoonJumpOnL"));
}

TEST_CASE(GameSettingsWaitForThePlayerAndHaveALimit) {
    ModServer m("coop_game_settings_limit");
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob"); // connected, not in the world yet
    CreateWorld(m.s, *a);
    CHECK(!a->WaitFor("mod_cfg", m.s, 200).has_value()); // nothing is forced: nothing to say
    CHECK_EQ(m.Load("limite", R"lua(
        coop.game.setSetting("Bob", "gCheats.InfiniteHealth", 1)
        print("largo=" .. tostring(pcall(coop.game.setSetting, "Alice", "gCheats." .. string.rep("x", 200), 1)))
        print("grande=" .. tostring(pcall(coop.game.setSetting, "Alice", "gCheats.Valor", 1e9)))
        local puestas = 0
        for i = 1, 70 do
            if pcall(coop.game.setSetting, "Alice", "gCheats.Prueba" .. i, i) then puestas = puestas + 1 end
        end
        print("puestas=" .. puestas)
        print("cambia=" .. tostring(pcall(coop.game.setSetting, "Alice", "gCheats.Prueba2", 5))) -- not a new one
        print("global=" .. tostring(pcall(coop.game.setSetting, "*", "gCheats.Otra", 1))) -- Alice has no room for it
        coop.game.clearSetting("Alice", "gCheats.Prueba1")
        coop.game.setSetting("Alice", "gCheats.Prueba1", 0) -- a place is free again
        local mias, suyas = 0, coop.game.settings("Bob")
        for _ in pairs(coop.game.settings("Alice")) do mias = mias + 1 end
        print("mias=" .. mias .. " suya=" .. tostring(suyas["gCheats.InfiniteHealth"]))
    )lua"),
             std::string());
    CHECK(m.Logged("largo=false"));
    CHECK(m.Logged("grande=false"));
    CHECK(m.Logged("puestas=" + std::to_string(kMaxModSettings)));
    CHECK(m.Logged("cambia=true"));
    CHECK(m.Logged("global=false"));
    CHECK(m.Logged("mias=" + std::to_string(kMaxModSettings) + " suya=1"));
    auto mine = a->WaitFor("mod_cfg", m.s);
    CHECK(mine.has_value());
    CHECK_EQ((*mine)["settings"].size(), (size_t)kMaxModSettings);
    CHECK(!(*mine)["settings"].contains("gCheats.InfiniteHealth")); // Bob's own
    CHECK(!b->WaitFor("mod_cfg", m.s, 200).has_value());            // not playing here: his game is his
    EnterWorld(m.s, *b);
    auto his = b->WaitFor("mod_cfg", m.s);
    CHECK(his.has_value()); // it waited for him
    CHECK_EQ((*his)["settings"].value("gCheats.InfiniteHealth", 0), 1);
    CHECK_EQ((*his)["settings"].size(), (size_t)1);
}

TEST_CASE(GamesReportItemsDeathsBossesAndKills) {
    ModServer m("coop_game_reports");
    CHECK_EQ(m.Load("oye", R"lua(
        coop.on("player_item", function(e) print("objeto " .. e.nick .. " " .. e.item .. " " .. e.itemKey) end)
        coop.on("player_death", function(e) print("muere " .. e.nick .. " en " .. e.scene) end)
        coop.on("boss_defeated", function(e) print("jefe " .. e.actorKey .. " por " .. e.nick) end)
        coop.on("enemy_killed", function(e)
            print("mata " .. e.nick .. " a " .. e.actorKey .. " params=" .. e.params .. " sala=" .. e.room ..
                  " x=" .. e.x)
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    a->Send({ { "t", "gev" }, { "k", "death" } }); // not in the world yet: nothing
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "sceneName", "South Clock Town" } });
    a->Send({ { "t", "gev" }, { "k", "item" }, { "id", 0x39 } });
    a->Send({ { "t", "gev" }, { "k", "death" } });
    a->Send({ { "t", "gev" }, { "k", "boss" }, { "actor", 0x0B } });
    a->Send({ { "t", "gev" }, { "k", "kill" }, { "actor", 0x0B }, { "params", 2 }, { "by", 2 }, { "room", 1 },
              { "pos", { 1.5, 2, 3 } } });
    a->Send({ { "t", "gev" }, { "k", "kill" }, { "actor", 0x0C }, { "params", 0 }, { "by", 9 }, { "room", 0 },
              { "pos", { 0, 0, 0 } } }); // nobody has id 9: the reporter gets it
    m.s.PumpFor(150);
    CHECK(m.Logged("objeto Alice 57 MASK_BUNNY"));
    CHECK(m.Logged("muere Alice en 111"));
    CHECK(m.Logged("jefe EN_DODONGO por Alice"));
    CHECK(m.Logged("mata Bob a EN_DODONGO params=2 sala=1 x=1.5"));
    CHECK(m.Logged("mata Alice a EN_FIREFLY params=0"));
    int deaths = 0;
    for (const std::string& line : m.s.log.Lines()) {
        deaths += line.find("muere Alice") != std::string::npos;
    }
    CHECK_EQ(deaths, 1);
    // Nonsense is counted as invalid, never passed on.
    a->Send({ { "t", "gev" }, { "k", "item" }, { "id", 5000 } });
    a->Send({ { "t", "gev" }, { "k", "raro" } });
    a->Send({ { "t", "gev" }, { "k", "kill" }, { "actor", 1 }, { "pos", "aqui" } });
    m.s.PumpFor(100);
    CHECK_EQ(m.s.server->Players().ByNick("Alice")->invalidMessages, (uint32_t)3);
}

TEST_CASE(StatsFillThePlayerAndFireChanges) {
    ModServer m("coop_game_stats");
    CHECK_EQ(m.Load("vida", R"lua(
        coop.on("player_stats", function(e)
            print("vida " .. e.nick .. " " .. e.prevHealth .. "->" .. e.health .. "/" .. e.maxHealth ..
                  " rupias " .. e.prevRupees .. "->" .. e.rupees .. " magia " .. e.magic)
        end)
        coop.commands.register("ver", { help = "x" }, function(ctx, args)
            local p = coop.players.get(args[1])
            return "vida=" .. tostring(p.health) .. " max=" .. tostring(p.maxHealth) .. " rupias=" .. tostring(p.rupees)
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    CreateWorld(m.s, *a);
    std::string out;
    server::ExecuteCommandLine(*m.s.server, nullptr, "ver Alice", &out);
    CHECK_EQ(out, std::string("vida=nil max=nil rupias=nil")); // unknown until its game says
    a->Send({ { "t", "stat" }, { "hp", 48 }, { "hpMax", 48 }, { "mp", 0 }, { "rupees", 10 } });
    a->Send({ { "t", "stat" }, { "hp", 32 }, { "hpMax", 48 }, { "mp", 0 }, { "rupees", 30 } });
    a->Send({ { "t", "stat" }, { "hp", 32 }, { "hpMax", 48 }, { "mp", 0 }, { "rupees", 30 } }); // no change: no news
    a->Send({ { "t", "stat" }, { "hp", -5 }, { "hpMax", 48 }, { "mp", 0 }, { "rupees", 30 } }); // invalid
    m.s.PumpFor(100);
    CHECK(m.Logged("vida Alice 48->48/48 rupias 10->10 magia 0"));
    CHECK(m.Logged("vida Alice 48->32/48 rupias 10->30 magia 0"));
    int events = 0;
    for (const std::string& line : m.s.log.Lines()) {
        events += line.find("[vida] vida Alice") != std::string::npos;
    }
    CHECK_EQ(events, 2);
    CHECK_EQ(m.s.server->Players().ByNick("Alice")->invalidMessages, (uint32_t)1);
    out.clear();
    server::ExecuteCommandLine(*m.s.server, nullptr, "ver Alice", &out);
    CHECK_EQ(out, std::string("vida=32 max=48 rupias=30"));
    for (int i = 0; i < 40; i++) { // a flood: the budget drops the rest
        a->Send({ { "t", "stat" }, { "hp", 1 + i }, { "hpMax", 48 }, { "mp", 0 }, { "rupees", 30 } });
    }
    m.s.PumpFor(100);
    events = 0;
    for (const std::string& line : m.s.log.Lines()) {
        events += line.find("[vida] vida Alice") != std::string::npos;
    }
    CHECK(events > 2);
    CHECK(events <= 2 + kStatBurst);
}

TEST_CASE(ScenesGoByNameInPlayersAndEvents) {
    ModServer m("coop_game_scenes");
    CHECK_EQ(m.Load("sitio", "coop.on('player_scene', function(e) print('llega a ' .. e.sceneKey) end)"),
             std::string());
    auto a = Join(m.s, "Alice");
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "sceneName", "South Clock Town" } });
    m.s.PumpFor(50);
    CHECK(m.Logged("llega a SOUTH_CLOCK_TOWN"));
    CHECK_EQ(m.Load("busca", R"lua(
        print("aqui=" .. #coop.players.inScene("SOUTH_CLOCK_TOWN") .. " alli=" .. #coop.players.inScene("TERMINA_FIELD"))
        print("num=" .. #coop.players.inScene(0x6F) .. " mala=" .. tostring(pcall(coop.players.inScene, "LUNA_ROTA")))
        print("clave=" .. coop.players.get("Alice").sceneKey)
        coop.players.teleport("Alice", { scene = "TERMINA_FIELD", x = 1, y = 2, z = 3 })
        print("tp=" .. tostring(pcall(coop.players.teleport, "Alice", { scene = "LUNA_ROTA", x = 1, y = 2, z = 3 })) ..
              tostring(pcall(coop.players.teleport, "Alice", { x = 1, y = 2, z = 3 })))
        print("ids=" .. coop.game.idOf("item", "BOW") .. " " .. coop.game.nameOf("actor", 11) .. " " ..
              tostring(coop.game.idOf("scene", "NADA")) .. " " .. coop.game.ids("scene").SOUTH_CLOCK_TOWN)
        print("mas=" .. tostring(coop.game.nameOf("item", 9999)) .. " " .. tostring(pcall(coop.game.ids, "cosa")))
    )lua"),
             std::string());
    CHECK(m.Logged("aqui=1 alli=0"));
    CHECK(m.Logged("num=1 mala=false"));
    CHECK(m.Logged("clave=SOUTH_CLOCK_TOWN"));
    CHECK(m.Logged("tp=falsefalse"));
    CHECK(m.Logged("ids=1 EN_DODONGO nil 111"));
    CHECK(m.Logged("mas=nil false"));
    auto tp = a->WaitFor("tp", m.s);
    CHECK(tp.has_value());
    CHECK_EQ((*tp)["entrance"].get<int>(), 0x2A << 9);
    CHECK_EQ((*tp)["scene"].get<int>(), 0x2D);
    CHECK_EQ((*tp)["room"].get<int>(), 0);
    CHECK(!a->WaitFor("tp", m.s, 200).has_value()); // the two that failed sent nothing
}

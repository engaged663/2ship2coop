// Mods against a real server: the session, chat and command hooks, and the players.* / chat.* functions.
#include "TestMods.h"

using namespace coop;
using namespace coop_test;

namespace {

int CountLogged(TestServer& s, const std::string& text) {
    int n = 0;
    for (const std::string& line : s.log.Lines()) {
        n += line.find(text) != std::string::npos;
    }
    return n;
}

} // namespace

TEST_CASE(ModsSeePlayersJoinAndLeave) {
    ModServer m("coop_srv_join");
    CHECK_EQ(m.Load("puerta", R"lua(
        coop.on("player_join", function(e)
            print("entra " .. e.nick .. " id=" .. e.player .. " ip=" .. e.ip)
            coop.chat.tell(e.player, "Bienvenido, " .. e.nick, "ok")
            coop.chat.broadcast(e.nick .. " ha llegado")
        end)
        coop.on("player_leave", function(e)
            print("sale " .. e.nick .. " (" .. e.reason .. ") quedan=" .. coop.players.count())
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    CHECK(m.Logged("entra Alice id=1 ip=127.0.0.1"));
    auto welcome = a->WaitFor("sys", m.s);
    CHECK(welcome.has_value());
    CHECK_EQ((*welcome)["text"].get<std::string>(), std::string("Bienvenido, Alice"));
    CHECK_EQ((*welcome)["level"].get<std::string>(), std::string("ok"));
    auto all = a->WaitFor("sys", m.s); // the broadcast
    CHECK(all.has_value());
    CHECK_EQ((*all)["text"].get<std::string>(), std::string("Alice ha llegado"));
    a->Close();
    m.s.PumpFor(1500);
    CHECK(m.Logged("sale Alice (desconectado) quedan=1")); // still counted inside the handler
}

TEST_CASE(ModsCanRejectAConnection) {
    ModServer m("coop_srv_reject");
    CHECK_EQ(m.Load("lista", R"lua(
        coop.on("player_connect", function(e)
            if e.nick ~= "Alice" then
                e.reason = "solo Alice"
                return false
            end
        end)
    )lua"),
             std::string());
    auto b = Connect(m.s);
    b->Hello("Bob");
    auto reject = b->WaitFor("reject", m.s);
    CHECK(reject.has_value());
    CHECK_EQ((*reject)["reason"].get<std::string>(), std::string("solo Alice"));
    auto a = Join(m.s, "Alice");
    CHECK_EQ(a->id, (uint8_t)1);
}

TEST_CASE(ModsFilterTheChat) {
    ModServer m("coop_srv_chat");
    CHECK_EQ(m.Load("filtro", R"lua(
        coop.on("chat", function(e)
            if e.text:find("spam") then return false end
            e.text = e.text:gsub("tonto", "*****")
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    a->Send({ { "t", "chat" }, { "text", "eres tonto" } });
    auto ev = a->WaitFor("chat", m.s);
    CHECK(ev.has_value());
    CHECK_EQ((*ev)["text"].get<std::string>(), std::string("eres *****"));
    a->Send({ { "t", "chat" }, { "text", "spam spam" } });
    CHECK(!a->WaitFor("chat", m.s, 300).has_value());
}

TEST_CASE(ModsCanBlockCommands) {
    ModServer m("coop_srv_command");
    CHECK_EQ(m.Load("guardia", R"lua(
        coop.on("command", function(e)
            print("cmd " .. e.name .. " de " .. e.nick .. " args=" .. #e.args .. " jugador=" .. e.player)
            if e.name == "list" then
                coop.chat.tell(e.player, "aquí no hay lista", "warn")
                return false
            end
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    a->Cmd("/list");
    auto reply = a->WaitFor("sys", m.s);
    CHECK(reply.has_value());
    CHECK_EQ((*reply)["text"].get<std::string>(), std::string("aquí no hay lista"));
    CHECK(m.Logged("cmd list de Alice args=0 jugador=1"));
    a->Cmd("/pm Alice hola");
    CHECK(a->WaitFor("pm", m.s).has_value()); // other commands still run
    a->Cmd("/kick Alice");
    CHECK(a->WaitForSys("error", m.s));
    CHECK(!m.Logged("cmd kick")); // refused before the mods hear of it
    m.s.server->ExecuteConsoleLine("say hola a todos");
    CHECK(m.Logged("cmd say de Servidor args=3 jugador=0")); // the console's commands too
}

TEST_CASE(ModsReadPlayersAndTheirPlace) {
    ModServer m("coop_srv_players");
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "entrance", 0xD800 },
              { "sceneName", "South Clock Town" } });
    a->SendState(0x6F, 0, 0xD800, 100.f, 0.f, 0.f);
    b->SendState(0x6F, 0, 0xD800, 150.f, 0.f, 0.f);
    b->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "entrance", 0xD800 },
              { "sceneName", "South Clock Town" } });
    m.s.PumpFor(100);
    CHECK_EQ(m.Load("mira", R"lua(
        local ids = coop.players.list()
        local p = coop.players.get("alice")
        print("ids=" .. #ids .. " id=" .. p.id .. " nick=" .. p.nick .. " escena=" .. p.scene ..
              " x=" .. math.floor(p.x) .. " sala=" .. p.room)
        print("nombre=" .. p.sceneName .. " mundo=" .. tostring(p.inWorld) .. " op=" .. tostring(p.op) ..
              " ip=" .. p.ip)
        print("find=" .. tostring(coop.players.find("BOB")) .. " nadie=" .. tostring(coop.players.find("Zed")))
        print("escena=" .. #coop.players.inScene(0x6F) .. " otra=" .. #coop.players.inScene(3) ..
              " mundo=" .. #coop.players.inWorld())
        print("cerca=" .. #coop.players.near("Alice", 60) .. " lejos=" .. #coop.players.near("Alice", 10))
        print("get=" .. tostring(coop.players.get(9)) .. tostring(coop.players.get("Zed")))
    )lua"),
             std::string());
    CHECK(m.Logged("ids=2 id=1 nick=Alice escena=111 x=100 sala=0"));
    CHECK(m.Logged("nombre=South Clock Town mundo=false op=false ip=127.0.0.1"));
    CHECK(m.Logged("find=2 nadie=nil"));
    CHECK(m.Logged("escena=2 otra=0 mundo=0"));
    CHECK(m.Logged("cerca=1 lejos=0"));
    CHECK(m.Logged("get=nilnil"));
}

TEST_CASE(ModsHearSceneChanges) {
    ModServer m("coop_srv_scene");
    CHECK_EQ(m.Load("mapa", R"lua(
        coop.on("player_scene", function(e)
            print(e.nick .. " " .. e.prevScene .. "/" .. e.prevRoom .. " -> " .. e.scene .. "/" .. e.room ..
                  " (" .. e.sceneName .. ")")
        end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "sceneName", "South Clock Town" } });
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 0 }, { "sceneName", "South Clock Town" },
              { "busy", true } }); // same place: no event
    a->Send({ { "t", "loc" }, { "scene", 0x6F }, { "room", 1 }, { "sceneName", "South Clock Town" } });
    m.s.PumpFor(100);
    CHECK(m.Logged("Alice -1/-1 -> 111/0 (South Clock Town)"));
    CHECK(m.Logged("Alice 111/0 -> 111/1 (South Clock Town)"));
    CHECK_EQ(CountLogged(m.s, "[mapa] Alice"), 2);
}

TEST_CASE(ModsModerateAndTeleport) {
    ModServer m("coop_srv_mod");
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    b->SendState(0x6F, 2, 0xD800, 10.f, 20.f, 30.f);
    m.s.PumpFor(50);
    Drain(m.s, { a.get(), b.get() });
    CHECK_EQ(m.Load("admin", R"lua(
        coop.players.setOp("Alice", true)
        print("op=" .. tostring(coop.players.isOp("Alice")) .. tostring(coop.players.isOp("Bob")))
        coop.players.teleport("Alice", "Bob")
        coop.players.teleport("Alice", { entrance = 0xD800, room = 1, x = 1, y = 2, z = 3, rot = 100 })
        print("mal=" .. tostring(pcall(coop.players.teleport, "Alice", "Alice")) ..
              tostring(pcall(coop.players.teleport, "Alice", { entrance = 0xFFFF, x = 0, y = 0, z = 0 })) ..
              tostring(pcall(coop.players.teleport, "Alice", { entrance = 0xD800, x = 0, y = 1e9, z = 0 })))
        coop.chat.say("Tatl", "¡Escucha!")
        coop.players.kick("Bob", "hasta luego")
    )lua"),
             std::string());
    CHECK(m.Logged("op=truefalse"));
    CHECK(m.Logged("mal=falsefalsefalse"));
    auto tp = a->WaitFor("tp", m.s);
    CHECK(tp.has_value());
    CHECK_EQ((*tp)["room"].get<int>(), 2);
    CHECK_EQ((*tp)["pos"][2].get<double>(), 30.0);
    CHECK_EQ((*tp)["target"].get<std::string>(), std::string("Bob"));
    auto tp2 = a->WaitFor("tp", m.s);
    CHECK(tp2.has_value());
    CHECK_EQ((*tp2)["entrance"].get<int>(), 0xD800);
    CHECK_EQ((*tp2)["room"].get<int>(), 1);
    CHECK_EQ((*tp2)["rot"].get<int>(), 100);
    CHECK(!a->WaitFor("tp", m.s, 200).has_value()); // the wrong ones never left
    auto chat = a->WaitFor("chat", m.s);
    CHECK(chat.has_value());
    CHECK_EQ((*chat)["from"].get<std::string>(), std::string("Tatl"));
    auto kicked = b->WaitFor("kicked", m.s);
    CHECK(kicked.has_value());
    CHECK_EQ((*kicked)["reason"].get<std::string>(), std::string("hasta luego"));
}

TEST_CASE(ModsBanPlayers) {
    ModServer m("coop_srv_ban");
    auto a = Join(m.s, "Alice");
    CHECK_EQ(m.Load("juez", "coop.players.ban('Alice', 'trampas') print('baneada')"), std::string());
    CHECK(a->WaitFor("kicked", m.s).has_value());
    CHECK_EQ(m.s.access.Bans().size(), (size_t)1);
    CHECK_EQ(m.s.access.Bans()[0].reason, std::string("trampas"));
}

TEST_CASE(ApiOnAPlayerWhoLeftFailsCleanly) {
    ModServer m("coop_srv_gone");
    CHECK_EQ(m.Load("tarde", R"lua(
        coop.on("player_join", function(e)
            coop.timer.after(300, function()
                local ok, err = pcall(coop.chat.tell, e.player, "sigues ahí?")
                print("ok=" .. tostring(ok) .. " get=" .. tostring(coop.players.get(e.player)))
            end)
        end)
    )lua"),
             std::string());
    {
        auto a = Join(m.s, "Alice");
        a->Close();
    }
    m.s.PumpFor(1800); // the disconnection is noticed, then the timer runs
    CHECK(m.Logged("ok=false get=nil"));
}

TEST_CASE(CommandPermissionsComeFromTheConfig) {
    server::ServerConfig cfg;
    cfg.commandPermissions = { { "tp", "op" }, { "list", "console" }, { "kick", "player" } };
    cfg.giftMax = 50;
    ModServer m("coop_srv_perms", cfg);
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    a->Cmd("/tp Bob");
    CHECK(a->WaitForSys("error", m.s)); // now for admins only
    a->Cmd("/list");
    CHECK(a->WaitForSys("error", m.s));
    a->Cmd("/help");
    auto help = a->WaitFor("sys", m.s);
    CHECK(help.has_value());
    CHECK(std::string((*help)["text"]).find("/list") == std::string::npos);
    CHECK(std::string((*help)["text"]).find("/kick") != std::string::npos);
    a->Cmd("/gift Bob 51");
    CHECK(a->WaitForSys("error", m.s)); // over giftMax
    a->Cmd("/gift Bob 50");
    CHECK(a->WaitFor("gift_debit", m.s).has_value());
    a->Cmd("/kick Bob");
    CHECK(b->WaitFor("kicked", m.s).has_value()); // opened to everyone
    TestServer plain; // another server: the overrides are gone
    auto c = Join(plain, "Cid");
    c->Cmd("/list");
    auto list = c->WaitFor("sys", plain);
    CHECK(list.has_value());
}

TEST_CASE(ModsHearGroupsAndActivities) {
    ModServer m("coop_srv_groups");
    CHECK_EQ(m.Load("social", R"lua(
        coop.on("group_change", function(e) print("grupo lider=" .. e.leader .. " miembros=" .. #e.members) end)
        coop.on("activity", function(e) print("act " .. e.nick .. " " .. e.state .. " " .. e.key) end)
    )lua"),
             std::string());
    auto a = Join(m.s, "Alice");
    auto b = Join(m.s, "Bob");
    CreateWorld(m.s, *a);
    EnterWorld(m.s, *b);
    a->Cmd("/invitar Bob");
    CHECK(b->WaitFor("invite", m.s).has_value());
    b->Cmd("/aceptar");
    m.s.PumpFor(100);
    CHECK(m.Logged("grupo lider=1 miembros=2"));
    a->Send({ { "t", "act" }, { "state", "start" }, { "key", "cofres" }, { "name", "Cofres" } });
    m.s.PumpFor(100);
    CHECK(m.Logged("act Alice start cofres"));
    b->Cmd("/dejargrupo");
    m.s.PumpFor(100);
    CHECK(m.Logged("grupo lider=0 miembros=0")); // a group of one is no group
}

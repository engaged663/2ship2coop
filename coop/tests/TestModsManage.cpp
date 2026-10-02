// Managing mods: stored data, which files the config loads, /mods and /mod, reloading.
#include "TestMods.h"

#include "server/CommandRegistry.h"

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

TEST_CASE(StorageKeepsDataAcrossReloadsAndRestarts) {
    TempDir data("coop_manage_data");
    server::ServerConfig cfg;
    cfg.mods.dataDir = data.path.string();
    const char* code = R"lua(
        local visitas = coop.storage.get("visitas", 0) + 1
        coop.storage.set("visitas", visitas)
        coop.storage.set("lista", { 1, 2, { nombre = "x" } })
        print("visitas=" .. visitas .. " claves=" .. table.concat(coop.storage.keys(), ",") ..
              " nombre=" .. coop.storage.get("lista")[3].nombre)
    )lua";
    {
        ModServer m("coop_manage_store", cfg);
        CHECK_EQ(m.Load("contador", code), std::string());
        CHECK(m.Logged("visitas=1 claves=lista,visitas nombre=x"));
        std::string err;
        CHECK(m.Mods().Reload("contador", &err));
        CHECK(m.Logged("visitas=2"));
    } // the server stops: the data is written
    CHECK(std::filesystem::exists(data.path / "contador.json"));
    ModServer again("coop_manage_store2", cfg);
    CHECK_EQ(again.Load("contador", code), std::string());
    CHECK(again.Logged("visitas=3"));
    CHECK_EQ(again.Load("otro", R"lua(
        print("ajeno=" .. tostring(coop.storage.get("visitas")))
        coop.storage.set("a", 1)
        print("borra=" .. tostring(coop.storage.remove("a")) .. tostring(coop.storage.remove("a")))
        coop.storage.set("b", 2)
        coop.storage.set("b", nil)
        print("nil borra=" .. tostring(coop.storage.get("b", "nada")) .. " claves=" .. #coop.storage.keys())
        coop.storage.save()
    )lua"),
             std::string());
    CHECK(again.Logged("ajeno=nil")); // each mod has its own data
    CHECK(again.Logged("borra=truefalse"));
    CHECK(again.Logged("nil borra=nada claves=0"));
}

TEST_CASE(StorageFileThatIsNotJsonStartsEmpty) {
    TempDir data("coop_manage_baddata");
    {
        std::ofstream f(data.path / "roto.json", std::ios::binary);
        f << "esto no es json";
    }
    server::ServerConfig cfg;
    cfg.mods.dataDir = data.path.string();
    ModServer m("coop_manage_badstore", cfg);
    CHECK_EQ(m.Load("roto", "print('valor=' .. tostring(coop.storage.get('x', 5)))"), std::string());
    CHECK(m.Logged("valor=5"));
    CHECK(m.Logged("roto.json")); // a warning names the file
}

TEST_CASE(ConfiguredScriptsLoadInTheOrderAsked) {
    TempDir dir("coop_manage_cfg");
    auto write = [&](const char* name, const char* code) {
        std::ofstream f(dir.path / name, std::ios::binary);
        f << code;
    };
    write("b.lua", "print('cargo b')");
    write("a.lua", "print('cargo a') coop.on('server_start', function() print('arrancado con ' .. #coop.mod.list()) end)");
    write("c.lua", "print('cargo c')");
    write("notas.txt", "no es un script");
    std::filesystem::create_directories(dir.path / "ejemplos");
    write("ejemplos/demo.lua", "print('cargo demo')");

    server::ServerConfig cfg;
    cfg.mods.scriptsDir = dir.path.string();
    cfg.mods.scripts = { "*", "!b.lua" };
    {
        TestServer s(cfg);
        std::vector<std::string> order;
        for (const std::string& line : s.log.Lines()) {
            if (line.find("] cargo ") != std::string::npos) {
                order.push_back(line.substr(line.find("cargo ")));
            }
        }
        CHECK_EQ(order.size(), (size_t)2); // not b (left out), not the subfolder, not the .txt
        CHECK_EQ(order[0], std::string("cargo a"));
        CHECK_EQ(order[1], std::string("cargo c"));
        CHECK(Logged(s, "arrancado con 2"));
    }
    cfg.mods.scripts = { "c", "ejemplos/demo.lua", "falta.lua", "a.lua", "c.lua" };
    {
        TestServer s(cfg);
        CHECK(Logged(s, "cargo demo"));
        CHECK(Logged(s, "falta.lua")); // a warning, and the rest still loads
        CHECK(Logged(s, "arrancado con 3"));
        CHECK(s.server->Mods().All()[0]->Info().name == "c");
        CHECK_EQ(CountLogged(s, "] cargo c"), 1); // named twice, loaded once
    }
    cfg.mods.enabled = false;
    {
        TestServer s(cfg);
        CHECK(!Logged(s, "] cargo "));
        CHECK(s.server->Mods().All().empty());
    }
}

TEST_CASE(BrokenScriptInTheConfigDoesNotStopTheOthers) {
    TempDir dir("coop_manage_broken_cfg");
    {
        std::ofstream bad(dir.path / "a_roto.lua", std::ios::binary);
        bad << "esto no es lua";
        std::ofstream good(dir.path / "b_bueno.lua", std::ios::binary);
        good << "print('cargo bueno')";
    }
    server::ServerConfig cfg;
    cfg.mods.scriptsDir = dir.path.string();
    cfg.mods.scripts = { "*" };
    TestServer s(cfg);
    CHECK(Logged(s, "a_roto"));       // why it did not load
    CHECK(Logged(s, "cargo bueno"));  // and the next one did
    CHECK_EQ(s.server->Mods().All().size(), (size_t)1);
    CHECK(s.server->IsRunning());
}

TEST_CASE(ModsCommandListsThemAndModManagesThem) {
    ModServer m("coop_manage_cmds");
    std::string path = m.Write("saludo.lua", "coop.mod.describe({ version = '1.0', description = 'saluda' }) "
                                             "coop.on('chat', function(e) e.text = 'v1 ' .. e.text end)");
    auto a = Join(m.s, "Alice");
    a->Cmd("/mods");
    auto none = a->WaitFor("sys", m.s);
    CHECK(none.has_value());
    m.s.server->ExecuteConsoleLine("mod load " + path);
    CHECK(m.Mods().Find("saludo") != nullptr);
    a->Cmd("/mods");
    auto list = a->WaitFor("sys", m.s);
    CHECK(list.has_value());
    CHECK(std::string((*list)["text"]).find("saludo (lua) v1.0 - saluda") != std::string::npos);
    a->Cmd("/mod reload saludo");
    CHECK(a->WaitForSys("error", m.s)); // players cannot
    m.s.server->ExecuteConsoleLine("op Alice");
    CHECK(a->WaitForSys("ok", m.s));
    // The file changes on disk; an admin reloads it from the game.
    m.Write("saludo.lua", "coop.on('chat', function(e) e.text = 'v2 ' .. e.text end)");
    a->Cmd("/mod reload saludo");
    CHECK(a->WaitForSys("ok", m.s));
    a->Send({ { "t", "chat" }, { "text", "hola" } });
    auto chat = a->WaitFor("chat", m.s);
    CHECK(chat.has_value());
    CHECK_EQ((*chat)["text"].get<std::string>(), std::string("v2 hola")); // the old handler is gone
    m.s.PumpFor(3100); // the chat/command rate limit refills
    a->Cmd("/mod unload saludo");
    CHECK(a->WaitForSys("error", m.s)); // console only
    a->Cmd("/mod load " + path);
    CHECK(a->WaitForSys("error", m.s));
    a->Cmd("/mod reload nadie");
    CHECK(a->WaitForSys("error", m.s));
    a->Cmd("/mod reload");
    CHECK(a->WaitForSys("ok", m.s)); // every mod
    m.s.server->ExecuteConsoleLine("mod unload saludo");
    CHECK(m.Mods().Find("saludo") == nullptr);
    m.s.server->ExecuteConsoleLine("mod");
    CHECK(m.Logged("/mod reload")); // usage
}

TEST_CASE(ModLoadFindsTheFilesOfTheModsFolder) {
    ModServer m("coop_manage_load");
    m.Write("extra.lua", "print('cargado extra')");
    m.Write("otro.lua", "print('cargado otro')");
    m.s.server->ExecuteConsoleLine("mod load extra"); // a file of the scripts' folder, without its extension...
    CHECK(m.Mods().Find("extra") != nullptr);
    m.s.server->ExecuteConsoleLine("mod load otro.lua"); // ...or with it
    CHECK(m.Mods().Find("otro") != nullptr);
    m.s.server->ExecuteConsoleLine("mod load nada");
    CHECK(m.Logged("nada")); // the file that is nowhere is named in the error
}

TEST_CASE(ModCanReloadItselfFromItsOwnCommand) {
    ModServer m("coop_manage_self");
    CHECK_EQ(m.Load("yo", R"lua(
        print("cargado yo")
        coop.commands.register("renacer", { help = "se recarga" }, function()
            coop.server.exec("mod reload yo")
            return "recargando"
        end)
        coop.commands.register("morir", { help = "se descarga" }, function()
            coop.server.exec("mod unload yo")
            return "adios"
        end)
    )lua"),
             std::string());
    std::string out;
    server::ExecuteCommandLine(*m.s.server, nullptr, "renacer", &out);
    CHECK_EQ(out, std::string("recargando")); // the handler finished on the state that was running it
    m.s.Pump();
    CHECK_EQ(CountLogged(m.s, "] cargado yo"), 2);
    CHECK(server::FindCommand("renacer") != nullptr);
    out.clear();
    server::ExecuteCommandLine(*m.s.server, nullptr, "morir", &out);
    CHECK_EQ(out, std::string("adios"));
    m.s.Pump();
    CHECK(m.Mods().Find("yo") == nullptr);
    CHECK(server::FindCommand("morir") == nullptr);
}

TEST_CASE(ReloadOfABrokenFileKeepsTheServerRunning) {
    ModServer m("coop_manage_broken");
    CHECK_EQ(m.Load("Mi Mod", "print('bien')"), std::string()); // the name is cleaned
    CHECK(m.Mods().Find("mi_mod") != nullptr);
    m.Write("Mi Mod.lua", "esto ya no es lua");
    std::string err;
    CHECK(!m.Mods().Reload("mi_mod", &err));
    CHECK(!err.empty());
    CHECK(m.Mods().Find("mi_mod") == nullptr); // gone, with the reason in err
    CHECK(!m.Mods().Reload("mi_mod", &err));   // and now there is nothing to reload
    m.s.Pump();
    CHECK(m.s.server->IsRunning());
}

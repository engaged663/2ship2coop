// Lua scripts as mods: loading, the coop table, the safe environment, limits and errors.
#include "TestMods.h"

#include "server/CommandRegistry.h"

using namespace coop;
using namespace coop_test;
using server::ModEvent;

TEST_CASE(LuaScriptLoadsAndPrintsToTheLog) {
    ModServer m("coop_lua_load");
    CHECK_EQ(m.Load("hola", "print('cargado', coop.mod.name(), 1 + 1)"), std::string());
    CHECK(m.Logged("[hola] cargado\thola\t2"));
    CHECK(m.Mods().Find("hola") != nullptr);
    CHECK_EQ(m.Mods().Find("hola")->Info().kind, std::string("lua"));
    CHECK(!m.Load("hola", "print('otra vez')").empty()); // the name is taken
    CHECK(!m.Logged("otra vez"));
}

TEST_CASE(LuaHandlersCanChangeAndCancelEvents) {
    ModServer m("coop_lua_events");
    CHECK_EQ(m.Load("filtro", R"lua(
        coop.on("chat", function(e)
            if e.text == "secreto" then return false end
            if e.text == "fuera" then e.cancel = true return end
            e.text = e.text:upper() .. " (" .. e.nick .. ")"
        end)
    )lua"),
             std::string());
    json e = { { "player", 1 }, { "nick", "Ana" }, { "text", "hola" } };
    CHECK(m.Mods().Fire(ModEvent::Chat, e));
    CHECK_EQ(e["text"].get<std::string>(), std::string("HOLA (Ana)"));
    CHECK(!e.contains("cancel"));
    e["text"] = "secreto";
    CHECK(!m.Mods().Fire(ModEvent::Chat, e));
    e["text"] = "fuera";
    CHECK(!m.Mods().Fire(ModEvent::Chat, e));
}

TEST_CASE(LuaOffEmitAndCustomEvents) {
    ModServer m("coop_lua_custom");
    CHECK_EQ(m.Load("a", R"lua(
        local id
        id = coop.on("banco:pago", function(e)
            e.total = e.cantidad * 2
            coop.off(id)
        end)
    )lua"),
             std::string());
    CHECK_EQ(m.Load("b", R"lua(
        local primero, cancelado = coop.emit("banco:pago", { cantidad = 5 })
        local segundo = coop.emit("banco:pago", { cantidad = 5 })
        print("total=" .. tostring(primero.total) .. " cancelado=" .. tostring(cancelado))
        print("despues=" .. tostring(segundo.total))
        print("malo=" .. tostring(pcall(coop.on, "chatt", function() end)))
        print("sin dos puntos=" .. tostring(pcall(coop.emit, "chat", {})))
    )lua"),
             std::string());
    CHECK(m.Logged("total=10 cancelado=false"));
    CHECK(m.Logged("despues=nil"));              // the handler unsubscribed itself
    CHECK(m.Logged("malo=false"));               // a misspelled event is an error, not silence
    CHECK(m.Logged("sin dos puntos=false"));     // a script cannot fake the server's own events
}

TEST_CASE(LuaTimersRunAndCancel) {
    ModServer m("coop_lua_timers");
    CHECK_EQ(m.Load("reloj", R"lua(
        local n = 0
        local cada
        cada = coop.timer.every(20, function()
            n = n + 1
            print("tic " .. n)
            if n == 3 then coop.timer.cancel(cada) end
        end)
        coop.timer.after(30, function() print("una vez") end)
        local nunca = coop.timer.after(30, function() print("nunca") end)
        coop.timer.cancel(nunca)
    )lua"),
             std::string());
    m.s.PumpFor(200);
    CHECK(m.Logged("tic 3"));
    CHECK(!m.Logged("tic 4"));
    CHECK(m.Logged("una vez"));
    CHECK(!m.Logged("nunca"));
}

TEST_CASE(LuaCallsTheApiAndSeesItsErrors) {
    ModServer m("coop_lua_api");
    CHECK_EQ(m.Load("api", R"lua(
        local info = coop.server.info()
        print("proto=" .. info.protocol .. " jugadores=" .. info.players)
        coop.server.log("desde lua", "warn")
        local ok, err = pcall(function() coop.server.log() end)
        print("ok=" .. tostring(ok) .. " err=" .. tostring(err))
        local ok2 = pcall(coop.server.log, function() end)
        print("ok2=" .. tostring(ok2))
    )lua"),
             std::string());
    CHECK(m.Logged("proto=" + std::to_string(kProtocolVersion) + " jugadores=0"));
    CHECK(m.Logged("[api] desde lua"));
    CHECK(m.Logged("ok=false err=api.lua:5: server.log:")); // the error names the script's line and the function
    CHECK(m.Logged("ok2=false"));                           // a function is not a JSON value
}

TEST_CASE(LuaSafeEnvironmentHasNoFilesOrProcesses) {
    ModServer m("coop_lua_safe");
    CHECK_EQ(m.Load("seguro", R"lua(
        print("io=" .. tostring(io) .. " exec=" .. tostring(os.execute) .. " load=" .. tostring(load) ..
              " dofile=" .. tostring(dofile) .. " debug=" .. tostring(debug) .. " package=" .. tostring(package))
        print("hora=" .. type(os.time()) .. " fecha=" .. type(os.date("%H")))
        print("libs=" .. type(string.format) .. type(table.insert) .. type(math.floor) .. type(utf8.char))
    )lua"),
             std::string());
    CHECK(m.Logged("io=nil exec=nil load=nil dofile=nil debug=nil package=nil"));
    CHECK(m.Logged("hora=number fecha=string"));
    CHECK(m.Logged("libs=functionfunctionfunctionfunction"));
}

TEST_CASE(LuaUnsafeModeOpensEverything) {
    server::ServerConfig cfg;
    cfg.mods.unsafeLua = true;
    ModServer m("coop_lua_unsafe", cfg);
    m.Write("lib/util.lua", "return { nombre = 'util' }");
    CHECK_EQ(m.Load("todo", "print('io=' .. type(io) .. ' exec=' .. type(os.execute) .. ' load=' .. type(load) .. "
                            "' util=' .. require('lib.util').nombre)"),
             std::string());
    CHECK(m.Logged("io=table exec=function load=function util=util"));
}

TEST_CASE(LuaRequireOnlyLoadsFromTheScriptsFolder) {
    ModServer m("coop_lua_require");
    m.Write("lib/util.lua", "local M = {} function M.doble(x) return x * 2 end print('util cargado') return M");
    CHECK_EQ(m.Load("usa", R"lua(
        local util = require("lib.util")
        local otra = require("lib.util")
        print("doble=" .. util.doble(21) .. " misma=" .. tostring(util == otra))
        print("fuera=" .. tostring(pcall(require, "..secreto")))
        print("falta=" .. tostring(pcall(require, "lib.nada")))
    )lua"),
             std::string());
    CHECK(m.Logged("doble=42 misma=true"));
    CHECK(m.Logged("fuera=false"));
    CHECK(m.Logged("falta=false"));
    int loads = 0;
    for (const std::string& line : m.s.log.Lines()) {
        loads += line.find("util cargado") != std::string::npos;
    }
    CHECK_EQ(loads, 1);
}

TEST_CASE(LuaLoadFailureLeavesNothingBehind) {
    ModServer m("coop_lua_fail");
    std::string err = m.Load("roto", R"lua(
        coop.on("chat", function(e) e.text = "pisado" end)
        coop.timer.every(10, function() print("sigo vivo") end)
        coop.commands.register("roto", { help = "x" }, function() return "hola" end)
        error("no arranco")
    )lua");
    CHECK(err.find("no arranco") != std::string::npos);
    CHECK(err.find("roto.lua") != std::string::npos);
    CHECK(m.Mods().Find("roto") == nullptr);
    CHECK(!m.Mods().Wants(ModEvent::Chat));
    CHECK(server::FindCommand("roto") == nullptr);
    m.s.PumpFor(60);
    CHECK(!m.Logged("sigo vivo"));
    CHECK(m.Load("sintaxis", "esto no es lua").find("sintaxis.lua") != std::string::npos);
    CHECK(!m.Mods().LoadFile(m.dir.File("no_existe.lua"), &err));
    CHECK(m.Mods().All().empty());
}

TEST_CASE(LuaHandlerErrorsAreLoggedAndTheServerGoesOn) {
    ModServer m("coop_lua_errors");
    CHECK_EQ(m.Load("falla", R"lua(
        coop.on("chat", function(e)
            local nada = nil
            return nada.campo
        end)
    )lua"),
             std::string());
    CHECK_EQ(m.Load("bueno", "coop.on('chat', function(e) e.text = 'sigue' end)"), std::string());
    json e = { { "player", 1 }, { "nick", "Ana" }, { "text", "hola" } };
    CHECK(m.Mods().Fire(ModEvent::Chat, e));
    CHECK_EQ(e["text"].get<std::string>(), std::string("sigue")); // the next mod still ran
    CHECK(m.Logged("[falla]"));
    CHECK(m.Logged("falla.lua:4"));
}

TEST_CASE(LuaInfiniteLoopIsAborted) {
    server::ServerConfig cfg;
    cfg.mods.scriptTimeoutMs = 100;
    ModServer m("coop_lua_loop", cfg);
    CHECK_EQ(m.Load("bucle", "coop.on('prueba:bucle', function() while true do end end)"), std::string());
    auto start = std::chrono::steady_clock::now();
    json e = json::object();
    m.Mods().FireCustom("prueba:bucle", e);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    CHECK(ms >= 90 && ms < 2000);
    CHECK(m.Logged("[bucle]"));
    CHECK(!m.Load("arranque", "while true do end").empty()); // the same at load time
    CHECK(m.Mods().Find("arranque") == nullptr);
}

TEST_CASE(LuaMemoryLimitStopsAGreedyScript) {
    server::ServerConfig cfg;
    cfg.mods.scriptMemoryMb = 8;
    ModServer m("coop_lua_mem", cfg);
    std::string err = m.Load("gloton", "local t = {} for i = 1, 100000000 do t[i] = string.rep('x', 1000) .. i end");
    CHECK(err.find("memory") != std::string::npos);
    CHECK_EQ(m.Load("normal", "print('sigo')"), std::string()); // the server and the other scripts are fine
    CHECK(m.Logged("[normal] sigo"));
}

TEST_CASE(LuaCommandsRegisterReplyAndGoAwayWithTheirMod) {
    ModServer m("coop_lua_cmd");
    CHECK_EQ(m.Load("cmds", R"lua(
        coop.commands.register("saluda", { usage = "/saluda <quien>", help = "saluda a alguien", minArgs = 1,
                                           aliases = { "hi" } },
            function(ctx, args)
                if ctx.isConsole then return "hola " .. args[1] .. " desde la consola" end
                return "hola " .. args[1] .. " de " .. ctx.nick .. " (" .. ctx.player .. ")", "ok"
            end)
        coop.commands.register("soloadmin", { help = "x", perm = "op" }, function() return "admin" end)
        coop.commands.register("mudo", {}, function() end)
        coop.commands.register("roto", {}, function() error("me rompo") end)
        print("repetido=" .. tostring(pcall(coop.commands.register, "help", {}, function() end)))
        print("malo=" .. tostring(pcall(coop.commands.register, "con espacios", {}, function() end)))
        print("quita=" .. tostring(coop.commands.unregister("mudo")) .. tostring(coop.commands.unregister("help")))
    )lua"),
             std::string());
    CHECK(m.Logged("repetido=false")); // a mod cannot take a command that exists
    CHECK(m.Logged("malo=false"));
    CHECK(m.Logged("quita=truefalse")); // its own command, never the server's
    CHECK(server::FindCommand("mudo") == nullptr);
    auto a = Join(m.s, "Alice");
    a->Cmd("/saluda Bob");
    auto reply = a->WaitFor("sys", m.s);
    CHECK(reply.has_value());
    CHECK_EQ((*reply)["text"].get<std::string>(), std::string("hola Bob de Alice (1)"));
    CHECK_EQ((*reply)["level"].get<std::string>(), std::string("ok"));
    a->Cmd("/hi Bob");
    CHECK(a->WaitFor("sys", m.s).has_value());
    a->Cmd("/saluda");
    CHECK(a->WaitForSys("error", m.s)); // usage
    a->Cmd("/soloadmin");
    CHECK(a->WaitForSys("error", m.s)); // no permission
    m.s.PumpFor(3100);                  // the chat/command rate limit refills
    a->Cmd("/roto");
    CHECK(a->WaitForSys("error", m.s)); // the player is told it failed...
    CHECK(m.Logged("me rompo"));        // ...and the log says why
    a->Cmd("/help");
    auto help = a->WaitFor("sys", m.s);
    CHECK(help.has_value());
    CHECK(std::string((*help)["text"]).find("/saluda <quien> - saluda a alguien") != std::string::npos);
    CHECK(std::string((*help)["text"]).find("/soloadmin") == std::string::npos);
    std::string out;
    server::ExecuteCommandLine(*m.s.server, nullptr, "saluda Cid", &out);
    CHECK_EQ(out, std::string("hola Cid desde la consola"));
    std::string err;
    CHECK(m.Mods().Unload("cmds", &err));
    CHECK(server::FindCommand("saluda") == nullptr);
    CHECK(server::FindCommand("hi") == nullptr);
    CHECK(server::FindCommand("help") != nullptr);
}

TEST_CASE(LuaModUnloadEventRunsOnlyInThatMod) {
    ModServer m("coop_lua_unload");
    CHECK_EQ(m.Load("uno", "coop.on('mod_unload', function() print('adios uno') end)"), std::string());
    CHECK_EQ(m.Load("dos", "coop.on('mod_unload', function() print('adios dos') end)"), std::string());
    std::string err;
    CHECK(m.Mods().Unload("uno", &err));
    CHECK(m.Logged("adios uno"));
    CHECK(!m.Logged("adios dos"));
}

TEST_CASE(LuaCoroutinesAndNestedEventsWork) {
    ModServer m("coop_lua_nested");
    CHECK_EQ(m.Load("anidado", R"lua(
        coop.on("prueba:fuera", function(e)
            local dentro = coop.emit("prueba:dentro", { n = e.n })
            e.resultado = dentro.n + 1
        end)
        coop.on("prueba:dentro", function(e) e.n = e.n * 10 end)
        local co = coroutine.wrap(function()
            for i = 1, 3 do coroutine.yield(i) end
        end)
        print("co=" .. co() .. co() .. co())
    )lua"),
             std::string());
    CHECK(m.Logged("co=123"));
    json e = { { "n", 4 } };
    CHECK(m.Mods().FireCustom("prueba:fuera", e));
    CHECK_EQ(e["resultado"].get<int>(), 41); // the same script ran inside itself
}

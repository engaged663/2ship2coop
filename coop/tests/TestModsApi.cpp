// The function table of the mod API and the server.* / mod.* functions, called as a mod would.
#include "TestMods.h"

#include "server/Mods/ModApi.h"

#include <set>

using namespace coop;
using namespace coop_test;
using server::ApiError;

namespace {

// Calls and returns the error text ("" when it worked).
std::string CallErr(TestServer& s, FakeMod* mod, const std::string& fn, const json& args, json* out = nullptr) {
    try {
        json result = s.server->Mods().Call(*mod, fn, args);
        if (out != nullptr) {
            *out = result;
        }
        return "";
    } catch (const ApiError& e) {
        return e.what();
    }
}

} // namespace

TEST_CASE(ApiTableHasDocsForEveryFunction) {
    std::set<std::string> names;
    for (const server::ApiDef* def : server::AllApis()) {
        std::string name = def->name;
        CHECK(names.insert(name).second);
        CHECK(name.find('.') != std::string::npos && name.find('.') == name.rfind('.'));
        CHECK(std::string(def->doc).size() > 10);
        CHECK(std::string(def->returns).size() > 0);
        CHECK(def->signature != nullptr && def->fn != nullptr);
    }
    CHECK(names.count("server.now") == 1);
    CHECK(server::FindApi("server.now") != nullptr);
    CHECK(server::FindApi("no.existe") == nullptr);
}

TEST_CASE(ApiUnknownFunctionsAndBadArgumentsFailCleanly) {
    TestServer s;
    FakeMod* mod = AddFake(s, "m");
    CHECK(CallErr(s, mod, "no.existe", json::array()).find("no.existe") != std::string::npos);
    std::string err = CallErr(s, mod, "server.log", json::array());
    CHECK(err.find("server.log") != std::string::npos);
    CHECK(err.find("text") != std::string::npos);
    CHECK(!CallErr(s, mod, "server.log", json::array({ 5 })).empty());            // not a text
    CHECK(!CallErr(s, mod, "server.log", json::array({ "x", "grave" })).empty()); // not a level
    CHECK(!CallErr(s, mod, "server.log", json::object()).empty());                // arguments must be a list
    CHECK(CallErr(s, mod, "server.log", json::array({ "todo bien" })).empty());
    CHECK(Logged(s, "[m] todo bien"));
}

TEST_CASE(ApiServerInfoNowAndExec) {
    TestServer s;
    FakeMod* mod = AddFake(s, "m");
    auto a = Join(s, "Alice");
    json info;
    CHECK(CallErr(s, mod, "server.info", json::array(), &info).empty());
    CHECK_EQ(info["protocol"].get<int>(), (int)kProtocolVersion);
    CHECK_EQ(info["players"].get<int>(), 1);
    CHECK_EQ(info["maxPlayers"].get<int>(), kMaxPlayers);
    CHECK_EQ(info["language"].get<std::string>(), std::string("es"));
    CHECK_EQ(info["mods"].get<int>(), 1);
    json now;
    CHECK(CallErr(s, mod, "server.now", json::array(), &now).empty());
    CHECK(now.is_number_integer());
    json reply;
    CHECK(CallErr(s, mod, "server.exec", json::array({ "list" }), &reply).empty());
    CHECK(reply.get<std::string>().find("Alice") != std::string::npos); // the answer comes back, it is not sent
    CHECK(!a->WaitFor("sys", s, 200).has_value());
    CHECK(CallErr(s, mod, "server.exec", json::array({ "/kick Alice fuera" })).empty()); // console rights
    CHECK(a->WaitFor("kicked", s).has_value());
}

TEST_CASE(ApiServerConfigNeverShowsSecrets) {
    server::ServerConfig cfg;
    cfg.password = "secreta";
    cfg.hostToken = "token";
    cfg.motd = "hola";
    TestServer s(cfg);
    FakeMod* mod = AddFake(s, "m");
    json v;
    CHECK(CallErr(s, mod, "server.config", json::array({ "motd" }), &v).empty());
    CHECK_EQ(v.get<std::string>(), std::string("hola"));
    CHECK(CallErr(s, mod, "server.config", json::array({ "maxPlayers" }), &v).empty());
    CHECK_EQ(v.get<int>(), kMaxPlayers);
    CHECK(!CallErr(s, mod, "server.config", json::array({ "password" })).empty());
    CHECK(!CallErr(s, mod, "server.config", json::array({ "hostToken" })).empty());
    CHECK(!CallErr(s, mod, "server.config", json::array({ "noExiste" })).empty());
    CHECK(CallErr(s, mod, "server.setConfig", json::array({ "motd", "nuevo" })).empty());
    CHECK_EQ(s.server->Config().motd, std::string("nuevo"));
    CHECK(CallErr(s, mod, "server.setConfig", json::array({ "maxPlayers", 2 })).empty());
    CHECK_EQ(s.server->Config().maxPlayers, 2);
    CHECK(CallErr(s, mod, "server.setConfig", json::array({ "password", "otra" })).empty()); // writable, never readable
    CHECK_EQ(s.server->Config().password, std::string("otra"));
    CHECK(!CallErr(s, mod, "server.setConfig", json::array({ "maxPlayers", 99 })).empty());
    CHECK(!CallErr(s, mod, "server.setConfig", json::array({ "port", 1 })).empty()); // not changeable while running
    CHECK_EQ(s.server->Config().maxPlayers, 2);
}

TEST_CASE(ApiModNameSettingsAndList) {
    server::ServerConfig cfg;
    cfg.mods.settings = { { "m", { { "saludo", "hola" }, { "veces", 3 } } } };
    TestServer s(cfg);
    FakeMod* mod = AddFake(s, "m");
    FakeMod* other = AddFake(s, "otro");
    json v;
    CHECK(CallErr(s, mod, "mod.name", json::array(), &v).empty());
    CHECK_EQ(v.get<std::string>(), std::string("m"));
    CHECK(CallErr(s, mod, "mod.setting", json::array({ "saludo" }), &v).empty());
    CHECK_EQ(v.get<std::string>(), std::string("hola"));
    CHECK(CallErr(s, mod, "mod.setting", json::array({ "falta", 7 }), &v).empty());
    CHECK_EQ(v.get<int>(), 7);
    CHECK(CallErr(s, other, "mod.setting", json::array({ "saludo" }), &v).empty());
    CHECK(v.is_null()); // each mod sees its own settings
    CHECK(CallErr(s, mod, "mod.describe",
                  json::array({ { { "version", "1.2" }, { "author", "yo" }, { "description", "prueba" } } }))
              .empty());
    CHECK(CallErr(s, mod, "mod.list", json::array(), &v).empty());
    CHECK_EQ(v.size(), (size_t)2);
    CHECK_EQ(v[0]["name"].get<std::string>(), std::string("m"));
    CHECK_EQ(v[0]["version"].get<std::string>(), std::string("1.2"));
    CHECK_EQ(v[0]["kind"].get<std::string>(), std::string("test"));
    CHECK_EQ(v[1]["name"].get<std::string>(), std::string("otro"));
}

TEST_CASE(ApiStopStopsTheServer) {
    TestServer s;
    FakeMod* mod = AddFake(s, "m");
    int stops = 0;
    mod->onEvent = [&](int, json& e, bool*) { stops += e["reason"].get<std::string>() == "mantenimiento"; };
    std::string err;
    s.server->Mods().Subscribe(*mod, "server_stop", 1, &err);
    CHECK(CallErr(s, mod, "server.stop", json::array({ "mantenimiento" })).empty());
    CHECK_EQ(stops, 1);
    s.PumpFor(1200);
    CHECK(!s.server->IsRunning());
}

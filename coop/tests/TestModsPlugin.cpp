// DLL plugins: the test plugin (tests/plugin/TestPlugin.cpp) and the SDK's example, loaded into a real server.
#include "TestMods.h"

#include "server/CommandRegistry.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using namespace coop;
using namespace coop_test;

namespace {

// The plugins are built next to coop-tests.
std::filesystem::path ExeDir() {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
#else
    return std::filesystem::read_symlink("/proc/self/exe").parent_path();
#endif
}

std::string PluginPath(const std::string& stem) {
#ifdef _WIN32
    return (ExeDir() / (stem + ".dll")).string();
#else
    return (ExeDir() / (stem + ".so")).string();
#endif
}

int CountLogged(TestServer& s, const std::string& text) {
    int n = 0;
    for (const std::string& line : s.log.Lines()) {
        n += line.find(text) != std::string::npos;
    }
    return n;
}

} // namespace

TEST_CASE(PluginLoadsAndUsesTheApi) {
    TestServer s;
    std::string err;
    CHECK(s.server->Mods().LoadFile(PluginPath("coop_test_plugin"), &err));
    CHECK(err.empty());
    server::Mod* mod = s.server->Mods().Find("coop_test_plugin");
    CHECK(mod != nullptr);
    CHECK_EQ(mod->Info().kind, std::string("plugin"));
    CHECK_EQ(mod->Info().title, std::string("Plugin de pruebas"));
    CHECK_EQ(mod->Info().version, std::string("1.2.3"));
    CHECK_EQ(mod->Info().description, std::string("usa toda la ABI"));
    CHECK(Logged(s, "[coop_test_plugin] cargado como coop_test_plugin"));
    auto a = Join(s, "Alice");
    a->Send({ { "t", "chat" }, { "text", "hola" } });
    auto chat = a->WaitFor("chat", s);
    CHECK(chat.has_value());
    CHECK_EQ((*chat)["text"].get<std::string>(), std::string("[p] hola"));
    a->Send({ { "t", "chat" }, { "text", "plugin-calla" } });
    CHECK(!a->WaitFor("chat", s, 300).has_value());
    a->Cmd("/pping uno dos");
    auto reply = a->WaitFor("sys", s);
    CHECK(reply.has_value());
    CHECK_EQ((*reply)["text"].get<std::string>(), std::string("pong Alice 2"));
    CHECK_EQ((*reply)["level"].get<std::string>(), std::string("ok"));
    a->Cmd("/pp");
    CHECK(a->WaitFor("sys", s).has_value());
    a->Cmd("/mods");
    auto mods = a->WaitFor("sys", s);
    CHECK(mods.has_value());
    CHECK(std::string((*mods)["text"]).find("coop_test_plugin (plugin) v1.2.3 - Plugin de pruebas: usa toda la ABI") !=
          std::string::npos);
}

TEST_CASE(PluginTimersDeferAndErrors) {
    TestServer s;
    std::string err;
    CHECK(s.server->Mods().LoadFile(PluginPath("coop_test_plugin"), &err));
    s.PumpFor(150);
    CHECK_EQ(CountLogged(s, "temporizador unico"), 1);
    CHECK(CountLogged(s, "tic del plugin") >= 3);
    json e = json::object();
    s.server->Mods().FireCustom("prueba:parar", e);
    int tics = CountLogged(s, "tic del plugin");
    s.PumpFor(100);
    CHECK_EQ(CountLogged(s, "tic del plugin"), tics);
    s.server->Mods().FireCustom("prueba:hilo", e);
    s.PumpFor(200); // the worker thread hands its work back to the server's thread
    CHECK(Logged(s, "de vuelta en el hilo del servidor"));
    json bad = json::object();
    s.server->Mods().FireCustom("prueba:error", bad);
    CHECK(bad["error"].get<std::string>().find("no.existe") != std::string::npos);
    s.server->Mods().FireCustom("prueba:lanza", e);
    CHECK(Logged(s, "fallo dentro del plugin")); // logged, and the server is still here
    CHECK(s.server->Mods().Unload("coop_test_plugin", &err));
    CHECK(Logged(s, "descargado"));
    CHECK(server::FindCommand("pping") == nullptr);
    CHECK(s.server->Mods().LoadFile(PluginPath("coop_test_plugin"), &err)); // and it can come back
    std::string reloadErr;
    CHECK(s.server->Mods().Reload("coop_test_plugin", &reloadErr));
    CHECK(s.server->Mods().Find("coop_test_plugin") != nullptr);
}

TEST_CASE(PluginsThatCannotLoadAreRefusedWithAReason) {
    TestServer s;
    std::string err;
    CHECK(!s.server->Mods().LoadFile(PluginPath("coop_test_plugin_badabi"), &err));
    CHECK(err.find("ABI") != std::string::npos);
    CHECK(s.server->Mods().All().empty());
    TempDir dir("coop_plugin_fake");
    std::string fake = dir.File("falso.dll");
    {
        std::ofstream f(fake, std::ios::binary);
        f << "esto no es una DLL";
    }
    CHECK(!s.server->Mods().LoadFile(fake, &err));
    CHECK(!err.empty());
    CHECK(!s.server->Mods().LoadFile(dir.File("no_esta.dll"), &err));
    CHECK(s.server->Mods().All().empty());
}

TEST_CASE(ExamplePluginLoads) {
    TestServer s;
    std::string err;
    CHECK(s.server->Mods().LoadFile(PluginPath("ejemplo-plugin"), &err));
    auto a = Join(s, "Alice");
    auto hello = a->WaitFor("sys", s);
    CHECK(hello.has_value());
    CHECK(std::string((*hello)["text"]).find("Alice") != std::string::npos);
    a->Cmd("/ping");
    auto reply = a->WaitFor("sys", s);
    CHECK(reply.has_value());
    CHECK(std::string((*reply)["text"]).find("pong (1 players") != std::string::npos);
}

TEST_CASE(ConfiguredPluginsLoadFromTheirFolder) {
    server::ServerConfig cfg;
    cfg.mods.pluginsDir = ExeDir().string();
    cfg.mods.plugins = { "coop_test_plugin" };
    TestServer s(cfg);
    CHECK(s.server->Mods().Find("coop_test_plugin") != nullptr);
}

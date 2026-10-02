// A plugin for coop-tests (TestModsPlugin.cpp): it uses every entry of the ABI through coop_plugin.hpp.
// Built twice: as it is, and with COOP_TEST_BAD_ABI (a plugin made for another version of the server).
#include "coop_plugin.hpp"

#include <thread>

#ifdef COOP_TEST_BAD_ABI

extern "C" COOP_PLUGIN_EXPORT uint32_t CoopPlugin_Abi(void) {
    return COOP_PLUGIN_ABI + 41;
}
extern "C" COOP_PLUGIN_EXPORT int32_t CoopPlugin_Load(const CoopApi*, CoopPluginInfo*) {
    return 1;
}

#else

COOP_PLUGIN("Plugin de pruebas", "1.2.3", "coop-tests", "usa toda la ABI")

static std::thread sWorker;

void CoopPluginMain(coop::Plugin& api) {
    api.Log("cargado como " + api.Call("mod.name").get<std::string>());
    api.On("chat", [](coop::Event& e) {
        std::string text = e["text"].get<std::string>();
        if (text == "plugin-calla") {
            e.Cancel();
        } else {
            e.Set("text", "[p] " + text);
        }
    });
    api.Command("pping", { { "help", "responde pong" }, { "aliases", { "pp" } } },
                [](const coop::CommandContext& ctx, const std::vector<std::string>& args) {
                    return coop::Reply{ "pong " + ctx.nick + " " + std::to_string(args.size()), "ok" };
                });
    api.After(20, [&api] { api.Log("temporizador unico"); });
    uint32_t every = api.Every(20, [&api] { api.Log("tic del plugin"); });
    api.On("prueba:parar", [&api, every](coop::Event&) { api.CancelTimer(every); });
    // Work done on a thread of the plugin's own comes back through Defer.
    api.On("prueba:hilo", [&api](coop::Event&) {
        if (sWorker.joinable()) {
            sWorker.join();
        }
        sWorker = std::thread([&api] { api.Defer([&api] { api.Log("de vuelta en el hilo del servidor"); }); });
    });
    api.On("prueba:error", [&api](coop::Event& e) {
        try {
            api.Call("no.existe");
        } catch (const coop::Error& err) {
            e.Set("error", std::string(err.what()));
        }
    });
    // An exception that escapes a handler must stop at the plugin's edge.
    api.On("prueba:lanza", [](coop::Event&) { throw std::runtime_error("fallo dentro del plugin"); });
    api.OnUnload([&api] {
        if (sWorker.joinable()) {
            sWorker.join();
        }
        api.Log("descargado");
    });
}

#endif

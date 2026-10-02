// Example plugin for the 2 Ship 2 Harkinian co-op server ("ejemplo" means "example").
//
// A plugin is a DLL the server loads from its "plugins" folder. It does the same as a Lua script (the same
// functions and the same events: see API.md), but in C++: use it when you need something Lua does not have (threads,
// sockets, a database, another library) or lots of speed.
//
// Building (with the SDK folder as it comes with the server):
//     cmake -S example -B build
//     cmake --build build --config Release
// and copy ejemplo-plugin.dll to the server's "plugins" folder. Full guide: PLUGINS.md.
#include "coop_plugin.hpp"

// Name, version, author and description: what the /mods command shows.
COOP_PLUGIN("Example plugin", "1.0", "2ship2coop", "greets, answers /ping and counts players")

// Runs once, when the server loads the plugin. Here you subscribe to events, register commands and start timers.
// `api` stays valid until the plugin is unloaded.
void CoopPluginMain(coop::Plugin& api) {
    api.Log("Example plugin loaded");

    // An event: someone enters the server. e["field"] reads an event field (API.md says which ones it has).
    api.On("player_join", [&api](coop::Event& e) {
        std::string nick = e["nick"].get<std::string>();
        // An API function: name and list of arguments, just like coop.chat.tell(player, text, level) in Lua.
        api.Call("chat.tell", { e["player"], "Hello, " + nick + ". This server has an example plugin.", "ok" });
    });

    // A new command: /ping (and /latido, its other name). What it returns is what the person who typed it reads.
    api.Command("ping", { { "usage", "/ping" }, { "help", "checks that the plugin responds" }, { "aliases", { "latido" } } },
                [&api](const coop::CommandContext& ctx, const std::vector<std::string>&) {
                    int players = api.Call("players.count").get<int>();
                    return coop::Reply{ "pong (" + std::to_string(players) + " players connected)", "ok" };
                });

    // A repeating timer: every 5 minutes it leaves a note in the server log.
    api.Every(5 * 60 * 1000, [&api] {
        try {
            coop::json info = api.Call("server.info");
            api.Log("Connected players: " + std::to_string(info["players"].get<int>()) + " of " +
                    std::to_string(info["maxPlayers"].get<int>()));
        } catch (const coop::Error& err) {
            // Call throws coop::Error when the server rejects the call (a wrong argument, a player who left...).
            api.Log(err.what(), COOP_LOG_WARN);
        }
    });

    // The last thing the plugin does before unloading (stop your own threads, close files...).
    api.OnUnload([&api] { api.Log("Example plugin unloaded"); });
}

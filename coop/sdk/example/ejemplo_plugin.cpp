// Plugin de ejemplo para el servidor co-op de 2 Ship 2 Harkinian.
//
// Un plugin es una DLL que el servidor carga desde su carpeta "plugins". Hace lo mismo que un script Lua (las mismas
// funciones y los mismos eventos: mira API.md), pero en C++: úsalo cuando necesites algo que Lua no tiene (hilos,
// sockets, una base de datos, otra librería) o mucha velocidad.
//
// Compilar (con la carpeta del SDK tal como viene con el servidor):
//     cmake -S example -B build
//     cmake --build build --config Release
// y copiar ejemplo-plugin.dll a la carpeta "plugins" del servidor. Guía completa: PLUGINS.md.
#include "coop_plugin.hpp"

// Nombre, versión, autor y descripción: lo que enseña el comando /mods.
COOP_PLUGIN("Plugin de ejemplo", "1.0", "2ship2coop", "saluda, responde a /ping y cuenta jugadores")

// Se ejecuta una vez, cuando el servidor carga el plugin. Aquí se suscribe uno a los eventos, registra comandos y
// arranca temporizadores. `api` sigue siendo válido hasta que el plugin se descarga.
void CoopPluginMain(coop::Plugin& api) {
    api.Log("Plugin de ejemplo cargado");

    // Un evento: alguien entra en el servidor. e["campo"] lee un campo del evento (API.md dice cuáles tiene).
    api.On("player_join", [&api](coop::Event& e) {
        std::string nick = e["nick"].get<std::string>();
        // Una función de la API: nombre y lista de argumentos, igual que coop.chat.tell(player, text, level) en Lua.
        api.Call("chat.tell", { e["player"], "Hola, " + nick + ". Este servidor tiene un plugin de ejemplo.", "ok" });
    });

    // Un comando nuevo: /ping (y /latido, su otro nombre). Lo que devuelve es lo que lee quien lo escribió.
    api.Command("ping", { { "usage", "/ping" }, { "help", "comprueba que el plugin responde" }, { "aliases", { "latido" } } },
                [&api](const coop::CommandContext& ctx, const std::vector<std::string>&) {
                    int players = api.Call("players.count").get<int>();
                    return coop::Reply{ "pong (" + std::to_string(players) + " jugadores conectados)", "ok" };
                });

    // Un temporizador que se repite: cada 5 minutos deja constancia en el registro del servidor.
    api.Every(5 * 60 * 1000, [&api] {
        try {
            coop::json info = api.Call("server.info");
            api.Log("Jugadores conectados: " + std::to_string(info["players"].get<int>()) + " de " +
                    std::to_string(info["maxPlayers"].get<int>()));
        } catch (const coop::Error& err) {
            // Call lanza coop::Error cuando el servidor rechaza la llamada (un argumento mal, un jugador que se fue...).
            api.Log(err.what(), COOP_LOG_WARN);
        }
    });

    // Lo último que hace el plugin antes de descargarse (parar hilos propios, cerrar archivos...).
    api.OnUnload([&api] { api.Log("Plugin de ejemplo descargado"); });
}

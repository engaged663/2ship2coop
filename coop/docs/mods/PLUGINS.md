# Plugins DLL

Un plugin es una biblioteca (`.dll` en Windows, `.so` en Linux) que el servidor carga desde su carpeta `plugins`.
Tiene **la misma API que los scripts** (las funciones y los eventos de [API.md](API.md)), pero en C++ o en C.

## Cuándo usar un plugin

Casi nunca hace falta: un script Lua hace lo mismo, no hay que compilarlo y se recarga en caliente. Un plugin sirve
cuando necesitas lo que Lua no tiene:

- hilos propios, sockets, HTTP, una base de datos u otra librería de C/C++;
- mucho cálculo por evento;
- código que ya tienes en C++.

A cambio: hay que compilarlo para el mismo sistema que el servidor, y **un fallo dentro de un plugin tumba el
servidor** (es código nativo, sin la red de seguridad de Lua). Instala solo plugins de quien te fíes.

## Qué trae el SDK

Junto al servidor va la carpeta `sdk`:

```
sdk/coop_plugin.h      la ABI en C (lo único imprescindible)
sdk/coop_plugin.hpp    un envoltorio C++17 cómodo (usa nlohmann/json)
sdk/nlohmann/json.hpp  la librería JSON que usa el envoltorio
sdk/example/           un plugin de ejemplo (ejemplo_plugin.cpp) con su CMakeLists.txt
```

## Compilar el ejemplo

En Windows hace falta Visual Studio 2022 (con «Desarrollo para el escritorio con C++») y CMake 3.16 o más nuevo. Desde
la carpeta `sdk`:

```
cmake -S example -B build
cmake --build build --config Release
```

Sale `build/Release/ejemplo-plugin.dll`. Cópiala a la carpeta `plugins` del servidor y arráncalo (o, con el servidor
en marcha, escribe en su consola `/mod load ejemplo-plugin`). `/mods` lo muestra y `/ping` le responde.

En Linux, lo mismo con g++ o clang: sale `build/ejemplo-plugin.so`.

Para tu propio plugin, copia la carpeta `example`, cambia el nombre del proyecto y de los `.cpp`, y deja la ruta a los
`.hpp` del SDK. El nombre del mod es el del archivo (`plugins/mi-plugin.dll` es `mi-plugin`).

**Reemplazar una DLL cargada**: Windows no deja sobrescribir una DLL en uso. En la consola del servidor:
`/mod unload mi-plugin`, copia la nueva, `/mod load mi-plugin`.

## Un plugin en C++ (`coop_plugin.hpp`)

```cpp
#include "coop_plugin.hpp"

// Una vez, en un .cpp: nombre, versión, autor y descripción (lo que enseña /mods).
COOP_PLUGIN("Mi plugin", "1.0", "yo", "da la bienvenida y cuenta muertes")

// Se ejecuta una vez, al cargar. Aquí se suscribe a eventos, registra comandos y arranca temporizadores.
void CoopPluginMain(coop::Plugin& api) {
    api.On("player_join", [&api](coop::Event& e) {
        api.Call("chat.tell", { e["player"], "Hola, " + e["nick"].get<std::string>(), "ok" });
    });

    api.On("chat", [](coop::Event& e) {
        if (e["text"].get<std::string>().find("spam") != std::string::npos) {
            e.Cancel();                        // en un evento que se puede cancelar
        }
    });

    api.Command("hora", { { "help", "la hora del mundo" } },
                [&api](const coop::CommandContext& ctx, const std::vector<std::string>& args) {
                    coop::json t = api.Call("world.time");
                    if (t.is_null()) {
                        return coop::Reply{ "Todavía no hay partida.", "warn" };
                    }
                    return coop::Reply{ "Día " + std::to_string(t["day"].get<int>()) + ", " +
                                        std::to_string(t["hour"].get<int>()) + " h", "ok" };
                });

    api.Every(60 * 1000, [&api] { api.Log("un minuto más"); });
    api.OnUnload([&api] { api.Log("adiós"); });
}
```

| Método | Qué hace |
|---|---|
| `Call(funcion, { args... })` | llama a una función de [API.md](API.md) (`"chat.tell"`, `"game.heal"`...); devuelve su resultado como `json`; lanza `coop::Error` si el servidor la rechaza. Si el único argumento es una tabla: `json::array({ tabla })` |
| `On(evento, fn)` | escucha un evento; `fn(coop::Event& e)`: `e["campo"]` lee, `e.Set("campo", valor)` cambia uno que se puede cambiar, `e.Cancel()` lo cancela. Devuelve la suscripción (0: no existe ese evento) |
| `Off(suscripcion)` | deja de escuchar |
| `After(ms, fn)` / `Every(ms, fn)` | temporizador de una vez / que se repite; devuelven su id |
| `CancelTimer(id)` | lo cancela |
| `Command(nombre, opts, fn)` | registra `/nombre`; `opts` como en Lua (`usage`, `help`, `perm`, `minArgs`, `aliases`); `fn(ctx, args)` devuelve `coop::Reply{ texto, nivel }` |
| `Defer(fn)` | desde **cualquier hilo**: `fn` se ejecuta en el hilo del servidor en su siguiente vuelta |
| `Log(texto, nivel)` | al registro del servidor (`COOP_LOG_INFO`, `COOP_LOG_WARN`, `COOP_LOG_ERROR`) |
| `OnUnload(fn)` | lo último antes de descargarse: para tus hilos, cierra tus archivos |

Un plugin puede escuchar los eventos entre mods (`"economia:pago"`), pero solo los scripts los lanzan
(`coop.emit`).

## Reglas

- **Hilos**: todo lo que el servidor llama (eventos, comandos, temporizadores) corre en su hilo, y desde ahí puedes
  llamar a la API. Desde un hilo tuyo solo `Defer` (en C, `defer`); el resto, dentro de lo que `Defer` ejecute.
- **Memoria y textos**: todo es UTF-8 terminado en `\0`. Un texto que devuelve el servidor vale hasta la siguiente
  llamada del plugin a la API; uno que devuelve el plugin desde una función suya debe seguir vivo hasta que el
  servidor vuelva a llamar a esa función. Nadie libera memoria del otro lado (el envoltorio C++ ya lo hace bien).
- **Excepciones**: ninguna excepción de C++ puede cruzar la ABI. El envoltorio las atrapa y las escribe en el
  registro; si escribes en C, no hay excepciones.
- **Errores**: una llamada que el servidor rechaza devuelve `{"ok":false,"error":"..."}` (en C++, `coop::Error`).

## La ABI en C (`coop_plugin.h`)

Lo que exporta la biblioteca:

| Función | Qué hace |
|---|---|
| `uint32_t CoopPlugin_Abi(void)` | devuelve `COOP_PLUGIN_ABI` (la versión de la ABI con la que se compiló) |
| `int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info)` | rellena `info` (nombre, versión, autor, descripción), se suscribe, registra... y devuelve 1 (0: no se carga) |
| `void CoopPlugin_Unload(void)` | opcional: lo último antes de descargarse |

Lo que le da el servidor (`CoopApi`; pasa `api->host` como primer argumento de cada función):

| Función | Qué hace |
|---|---|
| `call(host, "chat.tell", "[1, \"hola\"]")` | una función de la API; argumentos: una lista JSON (o `NULL`). Devuelve `{"ok":true,"value":...}` o `{"ok":false,"error":"..."}` |
| `on(host, evento, fn, user)` | escucha un evento; devuelve la suscripción (0: evento desconocido). `fn(user, evento, payloadJson)` devuelve `NULL` (nada que cambiar) o un objeto JSON con los campos cambiados y/o `"cancel": true` |
| `off(host, id)` | deja de escuchar |
| `timer(host, ms, repetir, fn, user)` | temporizador (`repetir` ≠ 0: se repite); devuelve su id |
| `cancel_timer(host, id)` | lo cancela |
| `command(host, nombre, optsJson, fn, user)` | registra `/nombre`; devuelve 1 (0: nombre ocupado o no válido). `fn(user, ctxJson, argsJson)` devuelve `NULL`, un texto o `{"text": "...", "level": "ok"}`; `ctxJson` = `{"player", "nick", "isConsole", "isOp"}` |
| `defer(host, fn, user)` | la única que se puede llamar desde otro hilo |
| `log(host, nivel, texto)` | al registro del servidor |

### Plugin mínimo en C

```c
#include "coop_plugin.h"

#include <stddef.h>

static const CoopApi* gApi;

/* payload: {"player":1,"nick":"Ana","ip":"..."}. Un plugin de verdad lo leería con una librería JSON. */
static const char* OnJoin(void* user, const char* event, const char* payload) {
    gApi->log(gApi->host, COOP_LOG_INFO, payload);
    return NULL; /* nada que cambiar */
}

COOP_PLUGIN_EXPORT uint32_t CoopPlugin_Abi(void) {
    return COOP_PLUGIN_ABI;
}

COOP_PLUGIN_EXPORT int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info) {
    gApi = api;
    info->name = "Plugin en C";
    info->version = "1.0";
    info->author = "yo";
    info->description = "anota en el registro quién entra";
    api->on(api->host, "player_join", OnJoin, NULL);
    api->call(api->host, "chat.broadcast", "[\"Un plugin en C acaba de cargarse\"]");
    return 1;
}
```

Se compila como cualquier DLL (`cl /LD minimo.c /I sdk` en una consola de Visual Studio, o
`gcc -shared -fPIC minimo.c -I sdk -o minimo.so` en Linux).

## Versiones

`COOP_PLUGIN_ABI` es la versión de la ABI (ahora 1). Un plugin compilado con otra versión no se carga: el registro
dice cuál tiene y cuál espera el servidor. Un servidor más nuevo puede añadir funciones al final de `CoopApi` sin
cambiar la versión (`api->size` dice cuántas tiene); las funciones de la API (`call`) y los eventos crecen sin tocar
la ABI.

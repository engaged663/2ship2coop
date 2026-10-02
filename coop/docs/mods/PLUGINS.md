# DLL plugins

A plugin is a library (`.dll` on Windows, `.so` on Linux) that the server loads from its `plugins` folder.
It has **the same API as scripts** (the functions and events of [API.md](API.md)), but in C++ or C.

## When to use a plugin

Almost never needed: a Lua script does the same, needs no compiling and hot-reloads. A plugin is useful
when you need what Lua does not have:

- your own threads, sockets, HTTP, a database or another C/C++ library;
- heavy computation per event;
- code you already have in C++.

In return: you have to compile it for the same system as the server, and **a fault inside a plugin takes the
server down** (it is native code, without Lua's safety net). Only install plugins from people you trust.

## What the SDK contains

The `sdk` folder comes with the server:

```
sdk/coop_plugin.h      the C ABI (the only essential piece)
sdk/coop_plugin.hpp    a convenient C++17 wrapper (uses nlohmann/json)
sdk/nlohmann/json.hpp  the JSON library the wrapper uses
sdk/example/           an example plugin (ejemplo_plugin.cpp) with its CMakeLists.txt
```

## Building the example

On Windows you need Visual Studio 2022 (with "Desktop development with C++") and CMake 3.16 or newer. From the
`sdk` folder:

```
cmake -S example -B build
cmake --build build --config Release
```

This produces `build/Release/ejemplo-plugin.dll`. Copy it to the server's `plugins` folder and start it (or, with the
server running, type `/mod load ejemplo-plugin` in its console). `/mods` shows it and `/ping` answers it.

On Linux, the same with g++ or clang: it produces `build/ejemplo-plugin.so`.

For your own plugin, copy the `example` folder, change the project name and the `.cpp` files, and keep the path to the
SDK's `.hpp`. The mod's name is the file's (`plugins/my-plugin.dll` is `my-plugin`).

**Replacing a loaded DLL**: Windows does not let you overwrite a DLL in use. In the server console:
`/mod unload my-plugin`, copy the new one, `/mod load my-plugin`.

## A C++ plugin (`coop_plugin.hpp`)

```cpp
#include "coop_plugin.hpp"

// Once, in one .cpp: name, version, author and description (what /mods shows).
COOP_PLUGIN("My plugin", "1.0", "me", "welcomes players and counts deaths")

// Runs once, on load. Here you subscribe to events, register commands and start timers.
void CoopPluginMain(coop::Plugin& api) {
    api.On("player_join", [&api](coop::Event& e) {
        api.Call("chat.tell", { e["player"], "Hello, " + e["nick"].get<std::string>(), "ok" });
    });

    api.On("chat", [](coop::Event& e) {
        if (e["text"].get<std::string>().find("spam") != std::string::npos) {
            e.Cancel();                        // in a cancelable event
        }
    });

    api.Command("time", { { "help", "the world's time" } },
                [&api](const coop::CommandContext& ctx, const std::vector<std::string>& args) {
                    coop::json t = api.Call("world.time");
                    if (t.is_null()) {
                        return coop::Reply{ "There is no game yet.", "warn" };
                    }
                    return coop::Reply{ "Day " + std::to_string(t["day"].get<int>()) + ", " +
                                        std::to_string(t["hour"].get<int>()) + " h", "ok" };
                });

    api.Every(60 * 1000, [&api] { api.Log("another minute"); });
    api.OnUnload([&api] { api.Log("goodbye"); });
}
```

| Method | What it does |
|---|---|
| `Call(function, { args... })` | calls a function from [API.md](API.md) (`"chat.tell"`, `"game.heal"`...); returns its result as `json`; throws `coop::Error` if the server rejects it. If the only argument is a table: `json::array({ table })` |
| `On(event, fn)` | listens to an event; `fn(coop::Event& e)`: `e["field"]` reads, `e.Set("field", value)` changes a writable one, `e.Cancel()` cancels it. Returns the subscription (0: no such event) |
| `Off(subscription)` | stops listening |
| `After(ms, fn)` / `Every(ms, fn)` | one-shot / repeating timer; they return its id |
| `CancelTimer(id)` | cancels it |
| `Command(name, opts, fn)` | registers `/name`; `opts` as in Lua (`usage`, `help`, `perm`, `minArgs`, `aliases`); `fn(ctx, args)` returns `coop::Reply{ text, level }` |
| `Defer(fn)` | from **any thread**: `fn` runs on the server's thread on its next turn |
| `Log(text, level)` | to the server log (`COOP_LOG_INFO`, `COOP_LOG_WARN`, `COOP_LOG_ERROR`) |
| `OnUnload(fn)` | the last thing before unloading: stop your threads, close your files |

A plugin can listen to events between mods (`"economy:payment"`), but only scripts fire them
(`coop.emit`).

## Rules

- **Threads**: everything the server calls (events, commands, timers) runs on its thread, and from there you can call
  the API. From a thread of your own, only `Defer` (in C, `defer`); the rest, inside whatever `Defer` runs.
- **Memory and strings**: everything is UTF-8 terminated by `\0`. A string the server returns is valid until the
  plugin's next call to the API; one the plugin returns from one of its functions must stay alive until the server
  calls that function again. Nobody frees memory from the other side (the C++ wrapper already does this right).
- **Exceptions**: no C++ exception may cross the ABI. The wrapper catches them and writes them to the log; if you
  write in C, there are no exceptions.
- **Errors**: a call the server rejects returns `{"ok":false,"error":"..."}` (in C++, `coop::Error`).

## The C ABI (`coop_plugin.h`)

What the library exports:

| Function | What it does |
|---|---|
| `uint32_t CoopPlugin_Abi(void)` | returns `COOP_PLUGIN_ABI` (the ABI version it was built with) |
| `int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info)` | fills `info` (name, version, author, description), subscribes, registers... and returns 1 (0: do not load) |
| `void CoopPlugin_Unload(void)` | optional: the last thing before unloading |

What the server gives it (`CoopApi`; pass `api->host` as the first argument of every function):

| Function | What it does |
|---|---|
| `call(host, "chat.tell", "[1, \"hello\"]")` | an API function; arguments: a JSON list (or `NULL`). Returns `{"ok":true,"value":...}` or `{"ok":false,"error":"..."}` |
| `on(host, event, fn, user)` | listens to an event; returns the subscription (0: unknown event). `fn(user, event, payloadJson)` returns `NULL` (nothing to change) or a JSON object with the changed fields and/or `"cancel": true` |
| `off(host, id)` | stops listening |
| `timer(host, ms, repeat, fn, user)` | timer (`repeat` ≠ 0: repeats); returns its id |
| `cancel_timer(host, id)` | cancels it |
| `command(host, name, optsJson, fn, user)` | registers `/name`; returns 1 (0: name taken or invalid). `fn(user, ctxJson, argsJson)` returns `NULL`, a text or `{"text": "...", "level": "ok"}`; `ctxJson` = `{"player", "nick", "isConsole", "isOp"}` |
| `defer(host, fn, user)` | the only one that can be called from another thread |
| `log(host, level, text)` | to the server log |

### A minimal plugin in C

```c
#include "coop_plugin.h"

#include <stddef.h>

static const CoopApi* gApi;

/* payload: {"player":1,"nick":"Ana","ip":"..."}. A real plugin would read it with a JSON library. */
static const char* OnJoin(void* user, const char* event, const char* payload) {
    gApi->log(gApi->host, COOP_LOG_INFO, payload);
    return NULL; /* nothing to change */
}

COOP_PLUGIN_EXPORT uint32_t CoopPlugin_Abi(void) {
    return COOP_PLUGIN_ABI;
}

COOP_PLUGIN_EXPORT int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info) {
    gApi = api;
    info->name = "C plugin";
    info->version = "1.0";
    info->author = "me";
    info->description = "logs who joins";
    api->on(api->host, "player_join", OnJoin, NULL);
    api->call(api->host, "chat.broadcast", "[\"A C plugin has just loaded\"]");
    return 1;
}
```

It is built like any DLL (`cl /LD minimal.c /I sdk` in a Visual Studio console, or
`gcc -shared -fPIC minimal.c -I sdk -o minimal.so` on Linux).

## Versions

`COOP_PLUGIN_ABI` is the ABI version (currently 1). A plugin built with another version is not loaded: the log
says which one it has and which one the server expects. A newer server may add functions at the end of `CoopApi`
without changing the version (`api->size` says how many there are); API functions (`call`) and events grow without
touching the ABI.

/* coop_plugin.h - C ABI of the plugins of the 2 Ship 2 Harkinian co-op server.
 *
 * A plugin is a shared library (.dll on Windows, .so on Linux) in the server's "plugins" folder that exports
 * CoopPlugin_Abi and CoopPlugin_Load. The functions and events are the same ones scripts use (see API.md):
 * coop.chat.tell(player, text) in Lua is call(host, "chat.tell", "[1, \"hello\"]") here.
 * C++ plugins: coop_plugin.hpp wraps all of this.
 *
 * Rules:
 *  - Everything runs on the server's thread. From another thread a plugin may only call CoopApi::defer.
 *  - Texts are UTF-8 and end in NUL. A text the server returns is valid until the plugin's next call to the API; a
 *    text a plugin returns from a callback must stay valid until that callback runs again. Nobody frees the other
 *    side's memory.
 *  - A crash inside a plugin is a crash of the server. */
#ifndef COOP_PLUGIN_H
#define COOP_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COOP_PLUGIN_ABI 1

#if defined(_WIN32)
#define COOP_PLUGIN_EXPORT __declspec(dllexport)
#else
#define COOP_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

/* An event. payloadJson: an object with the event's fields. Return NULL to change nothing, or a JSON object with
 * the fields to change and/or "cancel": true. */
typedef const char* (*CoopEventFn)(void* user, const char* event, const char* payloadJson);
typedef void (*CoopTimerFn)(void* user);
/* A command. ctxJson: {"player": id (missing for the console), "nick", "isConsole", "isOp"}; argsJson: ["a", "b"].
 * Return NULL (nothing to say), a text, or a JSON object {"text": "...", "level": "info"|"ok"|"warn"|"error"}. */
typedef const char* (*CoopCommandFn)(void* user, const char* ctxJson, const char* argsJson);

enum { COOP_LOG_INFO = 0, COOP_LOG_WARN = 1, COOP_LOG_ERROR = 2 };

typedef struct CoopApi {
    uint32_t abi;  /* the server's COOP_PLUGIN_ABI */
    uint32_t size; /* the server's sizeof(CoopApi): later servers may add functions at the end */
    void* host;    /* pass it back as the first argument of every function below */
    /* Calls a function of the API. argsJson: a JSON list (NULL: no arguments).
     * Returns {"ok":true,"value":...} or {"ok":false,"error":"..."} */
    const char* (*call)(void* host, const char* function, const char* argsJson);
    /* Subscribes to an event. Returns the subscription's id (0: unknown event). */
    uint32_t (*on)(void* host, const char* event, CoopEventFn fn, void* user);
    void (*off)(void* host, uint32_t subscription);
    /* fn runs once in ms milliseconds, or every ms when repeat is not 0. Returns the timer's id. */
    uint32_t (*timer)(void* host, int32_t ms, int32_t repeat, CoopTimerFn fn, void* user);
    void (*cancel_timer)(void* host, uint32_t timer);
    /* Registers /name. optsJson: {"usage", "help", "perm": "player"|"op"|"console", "minArgs", "aliases": [...]}
     * (NULL: the defaults). Returns 1, or 0 when the name is taken or not valid. */
    int32_t (*command)(void* host, const char* name, const char* optsJson, CoopCommandFn fn, void* user);
    /* The only function another thread may call: fn runs on the server's thread at its next tick. */
    void (*defer)(void* host, CoopTimerFn fn, void* user);
    void (*log)(void* host, int32_t level, const char* text);
} CoopApi;

typedef struct CoopPluginInfo {
    const char* name; /* shown by /mods; the mod's id is the file's name */
    const char* version;
    const char* author;
    const char* description;
} CoopPluginInfo;

#ifndef COOP_PLUGIN_HOST
/* What a plugin exports. */
COOP_PLUGIN_EXPORT uint32_t CoopPlugin_Abi(void);                                     /* return COOP_PLUGIN_ABI */
COOP_PLUGIN_EXPORT int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info); /* return 1 when ready */
COOP_PLUGIN_EXPORT void CoopPlugin_Unload(void);                                      /* optional */
#endif

#ifdef __cplusplus
}
#endif

#endif /* COOP_PLUGIN_H */

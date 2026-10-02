// coop_plugin.hpp - C++17 helper for plugins of the 2 Ship 2 Harkinian co-op server, over the C ABI of
// coop_plugin.h. Needs nlohmann/json.hpp (it ships next to this file in the SDK folder).
//
//   #include "coop_plugin.hpp"
//
//   COOP_PLUGIN("Mi plugin", "1.0", "yo", "saluda a quien entra")
//
//   void CoopPluginMain(coop::Plugin& api) {
//       api.On("player_join", [&api](coop::Event& e) {
//           api.Call("chat.tell", { e["player"], "Hola desde un plugin" });
//       });
//   }
//
// The functions and events are the ones of API.md: coop.chat.tell(player, text) in Lua is
// api.Call("chat.tell", { player, text }) here. Everything runs on the server's thread; from a thread of your own
// only Defer may be called.
#pragma once

#include "coop_plugin.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace coop {

using json = nlohmann::json;

// What Call throws when the server refuses the call: unknown function, wrong argument, no such player...
struct Error : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// An event, inside a handler.
class Event {
  public:
    explicit Event(json payload) : mPayload(std::move(payload)) {
    }
    // A field of the event (null when it has none by that name): e["nick"].get<std::string>()
    const json& operator[](const std::string& key) const {
        static const json kNull;
        auto it = mPayload.find(key);
        return it != mPayload.end() ? *it : kNull;
    }
    const json& Payload() const {
        return mPayload;
    }
    // Changes a field the reference marks as changeable ("text" of "chat"...).
    void Set(const std::string& key, json value) {
        mPayload[key] = value;
        mChanges[key] = std::move(value);
    }
    // Keeps what the event announces from happening (the events the reference marks as cancelable).
    void Cancel() {
        mChanges["cancel"] = true;
    }
    const json& Changes() const {
        return mChanges;
    }

  private:
    json mPayload;
    json mChanges = json::object();
};

// Who runs a command.
struct CommandContext {
    int player = 0; // its id; 0: the server's console
    std::string nick;
    bool isConsole = false;
    bool isOp = false;
};

// What a command answers (an empty text: nothing).
struct Reply {
    std::string text;
    std::string level = "info"; // info, ok, warn, error
};

class Plugin {
  public:
    using EventFn = std::function<void(Event&)>;
    using TimerFn = std::function<void()>;
    using CommandFn = std::function<Reply(const CommandContext&, const std::vector<std::string>&)>;

    explicit Plugin(const CoopApi* api) : mApi(api) {
    }
    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    // A function of the API: Call("chat.broadcast", { "hola" }). The arguments are a JSON list; when the only
    // argument is itself a table write json::array({ table }). Throws coop::Error when the server refuses it.
    json Call(const std::string& function, const json& args = json::array()) {
        std::string text = args.dump(-1, ' ', false, json::error_handler_t::replace);
        const char* raw = mApi->call(mApi->host, function.c_str(), text.c_str());
        json answer = json::parse(raw != nullptr ? raw : "", nullptr, false);
        if (!answer.is_object()) {
            throw Error(function + ": the server gave no answer");
        }
        if (!answer.value("ok", false)) {
            throw Error(answer.value("error", function + ": error"));
        }
        auto value = answer.find("value");
        return value != answer.end() ? *value : json();
    }

    // Listens to an event. Returns its subscription (0: no such event).
    uint32_t On(const std::string& event, EventFn fn) {
        Handler* handler = NewHandler();
        handler->event = std::move(fn);
        return mApi->on(mApi->host, event.c_str(), &Plugin::EventThunk, handler);
    }
    void Off(uint32_t subscription) {
        mApi->off(mApi->host, subscription);
    }

    uint32_t After(int ms, TimerFn fn) { // once
        return Timer(ms, false, std::move(fn));
    }
    uint32_t Every(int ms, TimerFn fn) {
        return Timer(ms, true, std::move(fn));
    }
    void CancelTimer(uint32_t timer) {
        mApi->cancel_timer(mApi->host, timer);
    }

    // Registers /name. opts: { {"usage", "/name <x>"}, {"help", "..."}, {"perm", "op"}, {"minArgs", 1},
    // {"aliases", {"otro"}} }. False when the name is taken or not valid.
    bool Command(const std::string& name, const json& opts, CommandFn fn) {
        Handler* handler = NewHandler();
        handler->command = std::move(fn);
        std::string text = opts.dump(-1, ' ', false, json::error_handler_t::replace);
        return mApi->command(mApi->host, name.c_str(), text.c_str(), &Plugin::CommandThunk, handler) != 0;
    }

    // From any thread: fn runs on the server's thread at its next tick (the result of work done elsewhere).
    void Defer(TimerFn fn) {
        mApi->defer(mApi->host, &Plugin::DeferThunk, new Deferred{ this, std::move(fn) });
    }

    void Log(const std::string& text, int level = COOP_LOG_INFO) const {
        mApi->log(mApi->host, level, text.c_str());
    }

    // fn runs when the plugin unloads (stop your threads, close your files).
    void OnUnload(TimerFn fn) {
        mUnload.push_back(std::move(fn));
    }
    void RunUnload() { // COOP_PLUGIN calls it
        for (auto it = mUnload.rbegin(); it != mUnload.rend(); ++it) {
            Guard([&] { (*it)(); });
        }
        mUnload.clear();
    }

  private:
    // What the server calls back: it lives as long as the plugin, so its address can be handed out.
    struct Handler {
        Plugin* plugin = nullptr;
        EventFn event;
        TimerFn timer;
        CommandFn command;
        std::string answer; // what the last call returned, kept until the next one
    };
    struct Deferred {
        Plugin* plugin;
        TimerFn fn;
    };

    Handler* NewHandler() {
        mHandlers.push_back(std::make_unique<Handler>());
        mHandlers.back()->plugin = this;
        return mHandlers.back().get();
    }

    uint32_t Timer(int ms, bool repeat, TimerFn fn) {
        Handler* handler = NewHandler();
        handler->timer = std::move(fn);
        return mApi->timer(mApi->host, ms, repeat ? 1 : 0, &Plugin::TimerThunk, handler);
    }

    // No C++ exception may cross the C ABI: it is logged and the call ends there.
    template <class Fn> bool Guard(Fn&& fn) const {
        try {
            fn();
            return true;
        } catch (const std::exception& e) {
            Log(e.what(), COOP_LOG_ERROR);
        } catch (...) {
            Log("unknown C++ exception", COOP_LOG_ERROR);
        }
        return false;
    }

    static const char* EventThunk(void* user, const char*, const char* payload) {
        Handler* h = (Handler*)user;
        bool changed = false;
        bool ok = h->plugin->Guard([&] {
            Event e(json::parse(payload != nullptr ? payload : "{}", nullptr, false));
            h->event(e);
            changed = !e.Changes().empty();
            h->answer = e.Changes().dump(-1, ' ', false, json::error_handler_t::replace);
        });
        return (ok && changed) ? h->answer.c_str() : nullptr;
    }

    static void TimerThunk(void* user) {
        Handler* h = (Handler*)user;
        h->plugin->Guard([&] { h->timer(); });
    }

    static const char* CommandThunk(void* user, const char* ctxJson, const char* argsJson) {
        Handler* h = (Handler*)user;
        bool said = false;
        bool ok = h->plugin->Guard([&] {
            json who = json::parse(ctxJson != nullptr ? ctxJson : "{}", nullptr, false);
            json list = json::parse(argsJson != nullptr ? argsJson : "[]", nullptr, false);
            CommandContext ctx;
            if (who.is_object()) {
                ctx.player = who.value("player", 0);
                ctx.nick = who.value("nick", std::string());
                ctx.isConsole = who.value("isConsole", false);
                ctx.isOp = who.value("isOp", false);
            }
            std::vector<std::string> args;
            if (list.is_array()) {
                for (const json& arg : list) {
                    if (arg.is_string()) {
                        args.push_back(arg.get<std::string>());
                    }
                }
            }
            Reply reply = h->command(ctx, args);
            said = !reply.text.empty();
            h->answer = json{ { "text", reply.text }, { "level", reply.level } }.dump(-1, ' ', false,
                                                                                      json::error_handler_t::replace);
        });
        return (ok && said) ? h->answer.c_str() : nullptr;
    }

    static void DeferThunk(void* user) {
        std::unique_ptr<Deferred> deferred((Deferred*)user);
        deferred->plugin->Guard([&] { deferred->fn(); });
    }

    const CoopApi* mApi;
    std::vector<std::unique_ptr<Handler>> mHandlers;
    std::vector<TimerFn> mUnload;
};

} // namespace coop

// What your plugin writes: it runs once, when the server loads the plugin. Subscribe, register commands, start
// timers. Throwing from it makes the load fail (the text goes to the server's log).
void CoopPluginMain(coop::Plugin& api);

// Once, in one .cpp of the plugin: its exports and what /mods says of it.
#define COOP_PLUGIN(name_, version_, author_, description_)                                              \
    static std::unique_ptr<coop::Plugin> gCoopPlugin;                                                    \
    extern "C" COOP_PLUGIN_EXPORT uint32_t CoopPlugin_Abi(void) {                                        \
        return COOP_PLUGIN_ABI;                                                                          \
    }                                                                                                    \
    extern "C" COOP_PLUGIN_EXPORT int32_t CoopPlugin_Load(const CoopApi* api, CoopPluginInfo* info) {    \
        if (api == nullptr || api->abi != COOP_PLUGIN_ABI) {                                             \
            return 0;                                                                                    \
        }                                                                                                \
        if (info != nullptr) {                                                                           \
            info->name = name_;                                                                          \
            info->version = version_;                                                                    \
            info->author = author_;                                                                      \
            info->description = description_;                                                            \
        }                                                                                                \
        gCoopPlugin = std::make_unique<coop::Plugin>(api);                                               \
        try {                                                                                            \
            CoopPluginMain(*gCoopPlugin);                                                                \
        } catch (const std::exception& e) {                                                              \
            gCoopPlugin->Log(e.what(), COOP_LOG_ERROR);                                                  \
            gCoopPlugin.reset();                                                                         \
            return 0;                                                                                    \
        } catch (...) {                                                                                  \
            gCoopPlugin.reset();                                                                         \
            return 0;                                                                                    \
        }                                                                                                \
        return 1;                                                                                        \
    }                                                                                                    \
    extern "C" COOP_PLUGIN_EXPORT void CoopPlugin_Unload(void) {                                         \
        if (gCoopPlugin) {                                                                               \
            gCoopPlugin->RunUnload();                                                                    \
            gCoopPlugin.reset();                                                                         \
        }                                                                                                \
    }

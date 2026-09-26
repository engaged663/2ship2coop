#pragma once
// Self-registration tables for the server. Each handler/command file registers itself with a macro,
// so adding a feature never requires touching Server.cpp:
//   COOP_SERVER_EVENT(myId, coop::ev::kChat, true, OnChat);          // JSON event handler
//   COOP_SERVER_STREAM(myId, coop::kStreamPlayerState, OnPose);      // binary stream handler
//   COOP_SERVER_ON_DISCONNECT(myId, OnLeave);                        // runs when any peer disconnects
//   COOP_SERVER_ON_TICK(myId, OnTick);                               // runs every Server::Tick
#include "common/Events.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace coop::server {

class Server;
struct RemoteClient;

using EventHandlerFn = void (*)(Server& server, RemoteClient& client, const json& ev);
using StreamHandlerFn = void (*)(Server& server, RemoteClient& client, uint8_t* data, size_t size);
using ClientHookFn = void (*)(Server& server, RemoteClient& client);
using TickHookFn = void (*)(Server& server);

struct EventHandlerEntry {
    EventHandlerFn fn = nullptr;
    bool requiresWelcome = true; // false only for "hello"
};

void RegisterEventHandler(const std::string& type, EventHandlerFn fn, bool requiresWelcome);
const EventHandlerEntry* FindEventHandler(const std::string& type);
void RegisterStreamHandler(uint8_t streamType, StreamHandlerFn fn);
StreamHandlerFn FindStreamHandler(uint8_t streamType);
void RegisterDisconnectHook(ClientHookFn fn);
const std::vector<ClientHookFn>& DisconnectHooks();
void RegisterTickHook(TickHookFn fn);
const std::vector<TickHookFn>& TickHooks();

struct HandlerRegistrar {
    HandlerRegistrar(const char* type, EventHandlerFn fn, bool requiresWelcome) {
        RegisterEventHandler(type, fn, requiresWelcome);
    }
    HandlerRegistrar(uint8_t streamType, StreamHandlerFn fn) {
        RegisterStreamHandler(streamType, fn);
    }
    explicit HandlerRegistrar(ClientHookFn fn) {
        RegisterDisconnectHook(fn);
    }
    explicit HandlerRegistrar(TickHookFn fn) {
        RegisterTickHook(fn);
    }
};

} // namespace coop::server

#define COOP_SERVER_EVENT(id, type, requiresWelcome, fn) \
    static coop::server::HandlerRegistrar id##_registrar(type, fn, requiresWelcome)
#define COOP_SERVER_STREAM(id, streamType, fn) \
    static coop::server::HandlerRegistrar id##_registrar((uint8_t)(streamType), fn)
#define COOP_SERVER_ON_DISCONNECT(id, fn) \
    static coop::server::HandlerRegistrar id##_registrar((coop::server::ClientHookFn)(fn))
#define COOP_SERVER_ON_TICK(id, fn) static coop::server::HandlerRegistrar id##_registrar((coop::server::TickHookFn)(fn))

#pragma once
// Routes network messages to the features that handle them. Features register themselves:
//   COOP_ON_EVENT(myId, coop::ev::kChat, OnChat);              // void OnChat(const coop::json& ev)
//   COOP_ON_STREAM(myId, coop::kStreamPlayerState, OnPose);    // void OnPose(const uint8_t* data, size_t size)
//   COOP_ON_LOST(myId, OnLost);                                // void OnLost(const std::string& reason)
// Several features may listen to the same event (e.g. "leave" is used by Session, Chat and Puppets).
#include "common/Events.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace coop::client {

using EventFn = void (*)(const json& ev);
using StreamFn = void (*)(const uint8_t* data, size_t size);
using LostFn = void (*)(const std::string& reason);

void AddEventHandler(const std::string& type, EventFn fn);
void AddStreamHandler(uint8_t streamType, StreamFn fn);
void AddLostHandler(LostFn fn);

// Drains NetClient and calls the handlers. Game thread, once per frame.
void ProcessNetwork();

struct Registrar {
    Registrar(const char* type, EventFn fn) {
        AddEventHandler(type, fn);
    }
    Registrar(uint8_t streamType, StreamFn fn) {
        AddStreamHandler(streamType, fn);
    }
    explicit Registrar(LostFn fn) {
        AddLostHandler(fn);
    }
};

} // namespace coop::client

#define COOP_ON_EVENT(id, type, fn) static coop::client::Registrar id##_coop_registrar(type, fn)
#define COOP_ON_STREAM(id, streamType, fn) static coop::client::Registrar id##_coop_registrar((uint8_t)(streamType), fn)
#define COOP_ON_LOST(id, fn) static coop::client::Registrar id##_coop_registrar((coop::client::LostFn)(fn))

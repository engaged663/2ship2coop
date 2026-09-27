#pragma once
// JSON events on the reliable channel: {"t": "<type>", ...fields}. Names live in Protocol.h.
#include "Protocol.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace coop {

using json = nlohmann::json;

json MakeEvent(const char* type);
// Never throws (invalid UTF-8 is replaced).
std::string SerializeEvent(const json& ev);
// True only for an object with a string "t" and size <= maxBytes. error gets a short reason.
// The server reads client events with the default limit; games read server events with kMaxServerEventBytes.
bool ParseEvent(const uint8_t* data, size_t size, json& out, std::string* error,
                size_t maxBytes = kMaxClientEventBytes);
std::string EventType(const json& ev);

// Typed getters: return def when the key is missing or has another type.
std::string GetString(const json& ev, const char* key, const std::string& def = "");
int64_t GetInt(const json& ev, const char* key, int64_t def = 0);
double GetNumber(const json& ev, const char* key, double def = 0.0);
bool GetBool(const json& ev, const char* key, bool def = false);

} // namespace coop

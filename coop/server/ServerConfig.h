#pragma once
// server.json: created with defaults on first run, edited by hand by the server owner.
#include "common/Protocol.h"

#include <string>

namespace coop::server {

struct ServerConfig {
    uint16_t port = kDefaultPort;
    int maxPlayers = kMaxPlayers; // clamped to 1..kMaxPlayers
    std::string password;         // empty = no password
    std::string motd = "Bienvenido al servidor co-op de Majora's Mask. Escribe /help para ver los comandos.";
    int handshakeTimeoutMs = kHandshakeTimeoutMs; // not stored in server.json (tests only)
    int giftTimeoutMs = kGiftTimeoutMs;           // not stored in server.json (tests only)
};

// Reads path; if it does not exist it is created with the defaults. False (with err) on invalid JSON.
// Fields with a wrong type keep their default and are reported in err while still returning true.
bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err);

} // namespace coop::server

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
    bool sharedEnemies = true; // sub-project C: enemies simulated by one player per room; false = each game its own
    // Sub-project D: the secret a headless host game shows to be accepted as the server's own. Empty = no hosts.
    // The server generates one for the hosts it starts; server.json may fix one for testing.
    std::string hostToken;
    // Sub-project D3: only games with the same executable as the first one accepted (actors copy code pointers).
    bool requireSameBuild = true;
    int handshakeTimeoutMs = kHandshakeTimeoutMs; // not stored in server.json (tests only)
    int giftTimeoutMs = kGiftTimeoutMs;           // not stored in server.json (tests only)
    // Shared world files, set by main.cpp (world.json and players/ next to the exe); "" = memory only (tests).
    std::string worldPath;
    std::string playersDir;
    // Not stored in server.json (tests only)
    int voteTimeoutMs = kSotVoteMs;
    int cycleComputeTimeoutMs = kCycleComputeMs;
    int worldCreateTimeoutMs = kWorldCreateMs;
    int worldSaveMs = kWorldSaveMs;
    int clockBroadcastMs = kClockBroadcastMs;
};

// Reads path; if it does not exist it is created with the defaults. False (with err) on invalid JSON.
// Fields with a wrong type keep their default and are reported in err while still returning true.
bool LoadOrCreateConfig(const std::string& path, ServerConfig& out, std::string* err);

} // namespace coop::server

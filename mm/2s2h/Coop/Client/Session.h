#pragma once
// Who we are and who else is on the server (kept up to date from welcome/join/leave/loc).
#include <cstdint>
#include <map>
#include <string>

namespace coop::client {

struct RemotePlayer {
    uint8_t id = 0;
    std::string nick;
    int16_t scene = -1;
    std::string sceneName;
};

bool Session_IsConnected(); // welcomed by the server
uint8_t Session_LocalId();
const std::string& Session_LocalNick();
const std::map<uint8_t, RemotePlayer>& Session_Players(); // everyone except us
const RemotePlayer* Session_FindPlayer(uint8_t id);

// Connects using the gCoop.* settings from the Co-op menu. False (with a notification) if the nick is invalid.
bool Session_ConnectFromSettings();

} // namespace coop::client

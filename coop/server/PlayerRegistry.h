#pragma once
// Every connected peer, from the moment ENet connects (handshaking) until it disconnects.
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace coop::server {

struct RemoteClient {
    uint32_t peer = 0;
    uint8_t id = 0; // 1..kMaxPlayers once welcomed
    std::string nick;
    std::string ip;
    bool welcomed = false;
    bool closing = false;    // kicked/rejected: ignore anything else it sends
    std::string leaveReason; // shown to the others when it leaves
    int64_t connectedAtMs = 0;

    // Last known location (from "loc" events and pose streams).
    int16_t scene = -1;
    int8_t room = -1;
    uint16_t entrance = 0;
    std::string sceneName;
    float pos[3] = { 0.f, 0.f, 0.f };
    int16_t rotY = 0;
    bool hasState = false;

    std::deque<int64_t> recentMessagesMs; // rate limiting
};

class PlayerRegistry {
  public:
    RemoteClient& Add(uint32_t peer, const std::string& ip, int64_t nowMs);
    void Remove(uint32_t peer);

    RemoteClient* ByPeer(uint32_t peer);
    RemoteClient* ByNick(const std::string& nick); // welcomed only, case-insensitive
    RemoteClient* ById(uint8_t id);                 // welcomed only

    int WelcomedCount() const;
    uint8_t AllocateId() const; // smallest free id in 1..kMaxPlayers, 0 if none
    std::vector<RemoteClient*> Welcomed();
    std::vector<RemoteClient*> All();

  private:
    std::vector<std::unique_ptr<RemoteClient>> mClients;
};

} // namespace coop::server

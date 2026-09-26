#pragma once
// Thin ENet wrapper. Channel 0 is sent reliable, channel 1 unreliable-sequenced (see Protocol.h).
// Not thread-safe: use each Transport from a single thread. Peers are identified by stable ids (never 0).
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace coop {

struct NetEvent {
    enum Type { Connect, Disconnect, Receive };
    Type type = Receive;
    uint32_t peer = 0;
    uint8_t channel = 0;
    std::vector<uint8_t> data;
};

class Transport {
  public:
    Transport() = default;
    ~Transport();
    Transport(const Transport&) = delete;
    Transport& operator=(const Transport&) = delete;

    bool Listen(uint16_t port, size_t maxPeers, std::string* err);
    // Starts connecting; a Connect event for *outPeer arrives from Service once established.
    bool Connect(const std::string& host, uint16_t port, uint32_t* outPeer, std::string* err);
    // Waits up to timeoutMs for the first event, then drains everything already queued.
    void Service(int timeoutMs, std::vector<NetEvent>& out);
    void Send(uint32_t peer, uint8_t channel, const void* data, size_t size);
    void Disconnect(uint32_t peer);    // graceful: queued reliable packets are delivered first
    void DisconnectNow(uint32_t peer); // immediate, no local Disconnect event
    void Flush();
    std::string PeerIp(uint32_t peer) const;
    uint32_t PeerRttMs(uint32_t peer) const;
    void Close();

  private:
    ENetPeer* Find(uint32_t peer) const;
    uint32_t Track(ENetPeer* peer);

    ENetHost* mHost = nullptr;
    std::unordered_map<uint32_t, ENetPeer*> mPeers;
    uint32_t mNextPeerId = 1;
    bool mEnetInitialized = false;
};

} // namespace coop

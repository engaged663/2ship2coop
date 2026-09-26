#pragma once
// Game-side connection to the co-op server. ENet runs on its own thread (ENet is not thread-safe,
// so only that thread touches it); the game thread talks to it through two locked queues.
#include "common/Events.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace coop::client {

enum class ConnState { Disconnected, Connecting, Handshaking, Connected };

struct Inbound {
    enum Kind { Event, Stream, Lost };
    Kind kind = Event;
    json event;                  // Event
    std::vector<uint8_t> stream; // Stream
    std::string reason;          // Lost: why the connection ended
};

class NetClient {
  public:
    static NetClient& Get();
    ~NetClient();

    void Connect(const std::string& host, uint16_t port, const std::string& nick, const std::string& password);
    void Disconnect();
    void SendEvent(const json& ev);
    void SendStream(std::vector<uint8_t> bytes);
    void Drain(std::vector<Inbound>& out); // game thread
    void MarkWelcomed();                   // called when "welcome" arrives

    ConnState State() const;
    std::string LastError() const;
    std::string ServerLabel() const; // "host:port"
    uint32_t PingMs() const;

  private:
    struct Outbound {
        uint8_t channel;
        std::vector<uint8_t> bytes;
    };

    void ThreadMain(std::string host, uint16_t port, std::string helloText);
    void StopThread();
    void Push(Inbound in);
    void Finish(const std::string& reason);

    std::thread mThread;
    std::atomic<bool> mRunning{ false };
    std::atomic<bool> mUserDisconnect{ false };
    std::atomic<ConnState> mState{ ConnState::Disconnected };
    std::atomic<uint32_t> mPingMs{ 0 };

    mutable std::mutex mMutex;
    std::deque<Inbound> mInbound;
    std::deque<Outbound> mOutbound;
    std::string mLastError;
    std::string mServerLabel;
};

} // namespace coop::client

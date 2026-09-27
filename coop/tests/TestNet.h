#pragma once
// Integration-test helpers: a real Server on a loopback port plus real ENet clients, all pumped
// from the test thread. Every live TestClient is pumped whenever anyone waits, so ENet keeps acking.
#include "TestMain.h"

#include "common/Events.h"
#include "common/PlayerState.h"
#include "common/Protocol.h"
#include "common/Transport.h"
#include "server/AccessLists.h"
#include "server/Logger.h"
#include "server/Server.h"
#include "server/ServerConfig.h"

#include <chrono>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace coop_test {

inline uint16_t NextTestPort() {
    static uint16_t port = 47800;
    return port++;
}

struct TestClient;

inline std::vector<TestClient*>& LiveClients() {
    static std::vector<TestClient*> clients;
    return clients;
}

struct TestServer {
    coop::server::ServerConfig cfg;
    coop::server::AccessLists access;
    coop::server::Logger log{ "", false };
    std::unique_ptr<coop::server::Server> server;

    explicit TestServer(coop::server::ServerConfig config = {}) : cfg(config) {
        cfg.port = NextTestPort();
        server = std::make_unique<coop::server::Server>(cfg, access, log);
        std::string err;
        if (!server->Start(&err)) {
            Fail(__FILE__, __LINE__, "server start failed: " + err);
        }
    }

    void Pump();
    void PumpFor(int ms) {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
        while (std::chrono::steady_clock::now() < end) {
            Pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

struct TestClient {
    coop::Transport transport;
    uint32_t peer = 0;
    bool connected = false;
    bool disconnected = false;
    std::deque<coop::json> events;
    std::deque<coop::PlayerState> streams;
    coop::json welcome;
    uint8_t id = 0;
    uint16_t seq = 0;

    TestClient() {
        LiveClients().push_back(this);
    }
    ~TestClient() {
        auto& live = LiveClients();
        for (size_t i = 0; i < live.size(); i++) {
            if (live[i] == this) {
                live.erase(live.begin() + i);
                break;
            }
        }
    }

    void Pump() {
        std::vector<coop::NetEvent> received;
        transport.Service(0, received);
        for (auto& e : received) {
            if (e.type == coop::NetEvent::Connect) {
                connected = true;
            } else if (e.type == coop::NetEvent::Disconnect) {
                disconnected = true;
            } else if (e.channel == coop::kChannelEvents) {
                coop::json ev;
                if (coop::ParseEvent(e.data.data(), e.data.size(), ev, nullptr, coop::kMaxServerEventBytes)) {
                    events.push_back(ev);
                }
            } else {
                coop::PlayerState st;
                if (coop::DecodePlayerState(e.data.data(), e.data.size(), st)) {
                    streams.push_back(st);
                }
            }
        }
    }

    void Send(const coop::json& ev) {
        std::string text = coop::SerializeEvent(ev);
        transport.Send(peer, coop::kChannelEvents, text.data(), text.size());
    }
    void SendRaw(const std::string& raw) {
        transport.Send(peer, coop::kChannelEvents, raw.data(), raw.size());
    }
    void Hello(const std::string& nick, const std::string& pass = "", int64_t proto = coop::kProtocolVersion) {
        Send({ { "t", "hello" }, { "proto", proto }, { "nick", nick }, { "pass", pass } });
    }
    void Cmd(const std::string& line) {
        Send({ { "t", "cmd" }, { "line", line } });
    }
    void SendState(int16_t scene, int8_t room = 0, uint16_t entrance = 0, float x = 0, float y = 0, float z = 0) {
        coop::PlayerState st;
        st.seq = ++seq;
        st.sceneId = scene;
        st.roomNum = room;
        st.entrance = entrance;
        st.pos[0] = x;
        st.pos[1] = y;
        st.pos[2] = z;
        auto bytes = coop::EncodePlayerState(st);
        transport.Send(peer, coop::kChannelStream, bytes.data(), bytes.size());
    }
    void Close() {
        transport.Close();
    }

    std::optional<coop::json> TakeEvent(const std::string& type, const char* level = nullptr) {
        for (size_t i = 0; i < events.size(); i++) {
            if (coop::EventType(events[i]) == type &&
                (level == nullptr || coop::GetString(events[i], "level") == level)) {
                coop::json ev = events[i];
                events.erase(events.begin() + i);
                return ev;
            }
        }
        return std::nullopt;
    }

    template <class Pred> bool WaitUntil(TestServer& s, int timeoutMs, Pred done) {
        auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (!done()) {
            if (std::chrono::steady_clock::now() >= end) {
                return false;
            }
            s.Pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return true;
    }

    std::optional<coop::json> WaitFor(const std::string& type, TestServer& s, int timeoutMs = 1000) {
        std::optional<coop::json> found;
        WaitUntil(s, timeoutMs, [&] { return (found = TakeEvent(type)).has_value(); });
        return found;
    }
    bool WaitForSys(const std::string& level, TestServer& s, int timeoutMs = 1000) {
        std::optional<coop::json> found;
        return WaitUntil(s, timeoutMs, [&] { return (found = TakeEvent("sys", level.c_str())).has_value(); });
    }
    std::optional<coop::PlayerState> WaitForStream(TestServer& s, int timeoutMs = 1000) {
        std::optional<coop::PlayerState> found;
        WaitUntil(s, timeoutMs, [&] {
            if (!streams.empty()) {
                found = streams.front();
                streams.pop_front();
            }
            return found.has_value();
        });
        return found;
    }
    bool WaitDisconnected(TestServer& s, int timeoutMs = 3000) {
        return WaitUntil(s, timeoutMs, [&] { return disconnected; });
    }
};

inline void TestServer::Pump() {
    if (server->IsRunning()) {
        server->Tick(0);
    }
    for (TestClient* client : LiveClients()) {
        client->Pump();
    }
}

// Connected at transport level, nothing sent yet.
inline std::unique_ptr<TestClient> Connect(TestServer& s) {
    auto client = std::make_unique<TestClient>();
    std::string err;
    if (!client->transport.Connect("127.0.0.1", s.cfg.port, &client->peer, &err)) {
        Fail(__FILE__, __LINE__, "connect failed: " + err);
    }
    if (!client->WaitUntil(s, 2000, [&] { return client->connected; })) {
        Fail(__FILE__, __LINE__, "connect timed out");
    }
    return client;
}

// Connected + welcomed.
inline std::unique_ptr<TestClient> Join(TestServer& s, const std::string& nick) {
    auto client = Connect(s);
    client->Hello(nick);
    auto welcome = client->WaitFor("welcome", s);
    if (!welcome.has_value()) {
        Fail(__FILE__, __LINE__, "no welcome for " + nick);
    }
    client->welcome = *welcome;
    client->id = (uint8_t)coop::GetInt(*welcome, "id");
    return client;
}

} // namespace coop_test

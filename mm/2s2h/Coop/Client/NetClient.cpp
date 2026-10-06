#include "NetClient.h"

#include "common/Protocol.h"
#include "common/BuildId.h"
#include "common/Transport.h"

#include <chrono>

namespace coop::client {

namespace {
constexpr size_t kMaxQueuedStreams = 64; // drop old poses if the network thread falls behind
constexpr auto kConnectTimeout = std::chrono::seconds(6);
} // namespace

NetClient& NetClient::Get() {
    static NetClient instance;
    return instance;
}

NetClient::~NetClient() {
    mUserDisconnect = true;
    StopThread();
}

void NetClient::Connect(const std::string& host, uint16_t port, const std::string& nick,
                        const std::string& password) {
    json hello = MakeEvent(ev::kHello);
    hello["proto"] = kProtocolVersion;
    hello["nick"] = nick;
    hello["pass"] = password;
    hello["build"] = BuildId();
    Start(host, port, hello);
}

void NetClient::ConnectHost(const std::string& host, uint16_t port, const std::string& token) {
    json hello = MakeEvent(ev::kHello);
    hello["proto"] = kProtocolVersion;
    hello["nick"] = kHostNick;
    hello["host"] = true;
    hello["token"] = token;
    hello["build"] = BuildId();
    Start(host, port, hello);
}

void NetClient::Start(const std::string& host, uint16_t port, const json& hello) {
    mUserDisconnect = true;
    StopThread(); // the old session's "Lost" stays queued so the game cleans up before the new welcome
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mOutbound.clear();
        mLastError.clear();
        mDisconnectReason.clear();
        mServerLabel = host + ":" + std::to_string(port);
    }
    mUserDisconnect = false;
    mState = ConnState::Connecting;
    mRunning = true;
    mThread = std::thread(&NetClient::ThreadMain, this, host, port, SerializeEvent(hello));
}

void NetClient::Disconnect(const std::string& reason) {
    if (!mThread.joinable()) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mDisconnectReason = reason;
    }
    mUserDisconnect = true;
    StopThread();
}

void NetClient::StopThread() {
    mRunning = false;
    if (mThread.joinable()) {
        mThread.join();
    }
}

void NetClient::SendEvent(const json& ev) {
    ConnState state = mState;
    if (state != ConnState::Handshaking && state != ConnState::Connected) {
        return;
    }
    std::string text = SerializeEvent(ev);
    std::lock_guard<std::mutex> lock(mMutex);
    mOutbound.push_back({ kChannelEvents, std::vector<uint8_t>(text.begin(), text.end()) });
}

void NetClient::SendStream(std::vector<uint8_t> bytes) {
    if (mState != ConnState::Connected) {
        return;
    }
    std::lock_guard<std::mutex> lock(mMutex);
    size_t streams = 0;
    for (auto& o : mOutbound) {
        streams += o.channel == kChannelStream ? 1 : 0;
    }
    if (streams >= kMaxQueuedStreams) {
        for (auto it = mOutbound.begin(); it != mOutbound.end(); ++it) {
            if (it->channel == kChannelStream) {
                mOutbound.erase(it);
                break;
            }
        }
    }
    mOutbound.push_back({ kChannelStream, std::move(bytes) });
}

void NetClient::Drain(std::vector<Inbound>& out) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& in : mInbound) {
        out.push_back(std::move(in));
    }
    mInbound.clear();
}

void NetClient::MarkWelcomed() {
    if (mState == ConnState::Handshaking) {
        mState = ConnState::Connected;
    }
}

ConnState NetClient::State() const {
    return mState;
}

std::string NetClient::LastError() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mLastError;
}

std::string NetClient::ServerLabel() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mServerLabel;
}

uint32_t NetClient::PingMs() const {
    return mPingMs;
}

void NetClient::Push(Inbound in) {
    std::lock_guard<std::mutex> lock(mMutex);
    mInbound.push_back(std::move(in));
}

void NetClient::Finish(const std::string& reason) {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLastError = reason;
        mOutbound.clear();
        Inbound lost;
        lost.kind = Inbound::Lost;
        lost.reason = reason;
        mInbound.push_back(std::move(lost));
    }
    mState = ConnState::Disconnected;
}

void NetClient::ThreadMain(std::string host, uint16_t port, std::string helloText) {
    Transport transport;
    uint32_t peer = 0;
    std::string error;
    if (!transport.Connect(host, port, &peer, &error)) {
        Finish(error);
        return;
    }

    bool connected = false;
    std::string serverReason; // from "reject" / "kicked"
    std::string lostReason;
    auto deadline = std::chrono::steady_clock::now() + kConnectTimeout;

    while (mRunning) {
        std::vector<NetEvent> events;
        transport.Service(5, events);
        for (auto& e : events) {
            if (e.type == NetEvent::Connect) {
                connected = true;
                mState = ConnState::Handshaking;
                transport.Send(peer, kChannelEvents, helloText.data(), helloText.size());
            } else if (e.type == NetEvent::Disconnect) {
                if (!serverReason.empty()) {
                    lostReason = serverReason;
                } else if (connected) {
                    lostReason = "Se ha perdido la conexión con el servidor.";
                } else {
                    lostReason = "No se pudo conectar con el servidor.";
                }
                mRunning = false;
            } else if (e.channel == kChannelEvents) {
                Inbound in;
                if (ParseEvent(e.data.data(), e.data.size(), in.event, nullptr, kMaxServerEventBytes)) {
                    std::string type = EventType(in.event);
                    if (type == ev::kReject || type == ev::kKicked) {
                        serverReason = GetString(in.event, "reason");
                    }
                    Push(std::move(in));
                }
            } else {
                Inbound in;
                in.kind = Inbound::Stream;
                in.stream = std::move(e.data);
                Push(std::move(in));
            }
        }
        if (!connected && std::chrono::steady_clock::now() > deadline) {
            lostReason = "No se pudo conectar con el servidor (tiempo agotado). Revisa la IP, el puerto y el firewall.";
            break;
        }

        std::deque<Outbound> outbound;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            outbound.swap(mOutbound);
        }
        for (auto& o : outbound) {
            transport.Send(peer, o.channel, o.bytes.data(), o.bytes.size());
        }
        mPingMs = transport.PeerRttMs(peer);
    }

    if (mUserDisconnect) {
        if (connected) {
            // What the game queued last (its final world changes and data, world_leave) goes out first: ENet delivers
            // queued reliable packets before the goodbye.
            std::deque<Outbound> last;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                last.swap(mOutbound);
            }
            for (auto& o : last) {
                if (o.channel == kChannelEvents) {
                    transport.Send(peer, o.channel, o.bytes.data(), o.bytes.size());
                }
            }
            // Polite goodbye so the others see us leave immediately.
            transport.Disconnect(peer);
            auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
            bool done = false;
            while (!done && std::chrono::steady_clock::now() < until) {
                std::vector<NetEvent> events;
                transport.Service(10, events);
                for (auto& e : events) {
                    done = done || e.type == NetEvent::Disconnect;
                }
            }
        }
        std::lock_guard<std::mutex> lock(mMutex); // released before Finish, which takes it again
        lostReason = mDisconnectReason.empty() ? "Te has desconectado del servidor." : mDisconnectReason;
    }
    transport.Close();
    Finish(lostReason);
}

} // namespace coop::client

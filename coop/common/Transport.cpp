#include "Transport.h"

#include "Protocol.h"

#include <enet/enet.h>

#include <mutex>

namespace coop {

namespace {

std::mutex gEnetMutex;
int gEnetUsers = 0;

bool AcquireEnet() {
    std::lock_guard<std::mutex> lock(gEnetMutex);
    if (gEnetUsers == 0 && enet_initialize() != 0) {
        return false;
    }
    gEnetUsers++;
    return true;
}

void ReleaseEnet() {
    std::lock_guard<std::mutex> lock(gEnetMutex);
    if (--gEnetUsers == 0) {
        enet_deinitialize();
    }
}

// A silent peer is dropped after 3-10 s instead of ENet's default 5-30 s.
constexpr enet_uint32 kPeerTimeoutMinMs = 3000;
constexpr enet_uint32 kPeerTimeoutMaxMs = 10000;

uint32_t IdOf(const ENetPeer* peer) {
    return (uint32_t)(uintptr_t)peer->data;
}

void SetError(std::string* err, const std::string& text) {
    if (err != nullptr) {
        *err = text;
    }
}

} // namespace

Transport::~Transport() {
    Close();
}

bool Transport::Listen(uint16_t port, size_t maxPeers, std::string* err) {
    Close();
    if (!AcquireEnet()) {
        SetError(err, "No se pudo inicializar la red (enet_initialize)");
        return false;
    }
    mEnetInitialized = true;
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    mHost = enet_host_create(&address, maxPeers, kChannelCount, 0, 0);
    if (mHost == nullptr) {
        SetError(err, "No se pudo abrir el puerto UDP " + std::to_string(port) + " (¿ya está en uso?)");
        Close();
        return false;
    }
    mHost->maximumPacketSize = kMaxPacketBytes; // nothing legitimate is bigger; don't reassemble floods
    enet_host_compress_with_range_coder(mHost); // D3: actor memory is mostly repeated bytes
    return true;
}

bool Transport::Connect(const std::string& host, uint16_t port, uint32_t* outPeer, std::string* err) {
    Close();
    if (!AcquireEnet()) {
        SetError(err, "No se pudo inicializar la red (enet_initialize)");
        return false;
    }
    mEnetInitialized = true;
    mHost = enet_host_create(nullptr, 1, kChannelCount, 0, 0);
    if (mHost == nullptr) {
        SetError(err, "No se pudo crear el socket UDP");
        Close();
        return false;
    }
    enet_host_compress_with_range_coder(mHost); // the server compresses too (same library on both ends)
    ENetAddress address;
    if (enet_address_set_host(&address, host.c_str()) != 0) {
        SetError(err, "No se pudo resolver la dirección '" + host + "'");
        Close();
        return false;
    }
    address.port = port;
    ENetPeer* peer = enet_host_connect(mHost, &address, kChannelCount, 0);
    if (peer == nullptr) {
        SetError(err, "No se pudo iniciar la conexión");
        Close();
        return false;
    }
    uint32_t id = Track(peer);
    if (outPeer != nullptr) {
        *outPeer = id;
    }
    return true;
}

void Transport::Service(int timeoutMs, std::vector<NetEvent>& out) {
    if (mHost == nullptr) {
        return;
    }
    ENetEvent ev;
    enet_uint32 wait = timeoutMs > 0 ? (enet_uint32)timeoutMs : 0;
    while (mHost != nullptr && enet_host_service(mHost, &ev, wait) > 0) {
        wait = 0;
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t id = IdOf(ev.peer);
                if (id == 0) {
                    id = Track(ev.peer);
                }
                enet_peer_timeout(ev.peer, 0, kPeerTimeoutMinMs, kPeerTimeoutMaxMs);
                NetEvent e;
                e.type = NetEvent::Connect;
                e.peer = id;
                out.push_back(std::move(e));
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE: {
                NetEvent e;
                e.type = NetEvent::Receive;
                e.peer = IdOf(ev.peer);
                e.channel = ev.channelID;
                e.data.assign(ev.packet->data, ev.packet->data + ev.packet->dataLength);
                enet_packet_destroy(ev.packet);
                if (e.peer != 0) {
                    out.push_back(std::move(e));
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: {
                uint32_t id = IdOf(ev.peer);
                if (id != 0) {
                    mPeers.erase(id);
                    ev.peer->data = nullptr;
                    NetEvent e;
                    e.type = NetEvent::Disconnect;
                    e.peer = id;
                    out.push_back(std::move(e));
                }
                break;
            }
            default:
                break;
        }
    }
}

void Transport::Send(uint32_t peer, uint8_t channel, const void* data, size_t size) {
    ENetPeer* p = Find(peer);
    if (p == nullptr || channel >= kChannelCount) {
        return;
    }
    enet_uint32 flags = channel == kChannelEvents ? ENET_PACKET_FLAG_RELIABLE : 0;
    ENetPacket* packet = enet_packet_create(data, size, flags);
    if (packet != nullptr && enet_peer_send(p, channel, packet) < 0) {
        enet_packet_destroy(packet);
    }
}

void Transport::Disconnect(uint32_t peer) {
    if (ENetPeer* p = Find(peer)) {
        enet_peer_disconnect_later(p, 0);
    }
}

void Transport::DisconnectNow(uint32_t peer) {
    if (ENetPeer* p = Find(peer)) {
        enet_peer_disconnect_now(p, 0);
        p->data = nullptr;
        mPeers.erase(peer);
    }
}

void Transport::Flush() {
    if (mHost != nullptr) {
        enet_host_flush(mHost);
    }
}

std::string Transport::PeerIp(uint32_t peer) const {
    ENetPeer* p = Find(peer);
    if (p == nullptr) {
        return "";
    }
    char buffer[64] = {};
    if (enet_address_get_host_ip(&p->address, buffer, sizeof(buffer)) != 0) {
        return "";
    }
    return buffer;
}

uint32_t Transport::PeerRttMs(uint32_t peer) const {
    ENetPeer* p = Find(peer);
    return p != nullptr ? p->roundTripTime : 0;
}

void Transport::Close() {
    if (mHost != nullptr) {
        // Tell remote ends right away so they don't wait for a timeout.
        for (auto& entry : mPeers) {
            entry.second->data = nullptr;
            enet_peer_disconnect_now(entry.second, 0);
        }
        mPeers.clear();
        enet_host_flush(mHost);
        enet_host_destroy(mHost);
        mHost = nullptr;
    }
    if (mEnetInitialized) {
        mEnetInitialized = false;
        ReleaseEnet();
    }
}

ENetPeer* Transport::Find(uint32_t peer) const {
    auto it = mPeers.find(peer);
    return it != mPeers.end() ? it->second : nullptr;
}

uint32_t Transport::Track(ENetPeer* peer) {
    uint32_t id = mNextPeerId++;
    if (mNextPeerId == 0) {
        mNextPeerId = 1;
    }
    peer->data = (void*)(uintptr_t)id;
    mPeers[id] = peer;
    return id;
}

} // namespace coop

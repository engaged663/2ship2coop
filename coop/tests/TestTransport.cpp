#include "TestMain.h"

#include "common/Protocol.h"
#include "common/Transport.h"

#include <string>
#include <vector>

TEST_CASE(TransportLoopbackReliableAndStream) {
    coop::Transport server;
    coop::Transport client;
    std::string err;
    CHECK(server.Listen(47700, 4, &err));
    uint32_t clientPeer = 0;
    CHECK(client.Connect("127.0.0.1", 47700, &clientPeer, &err));
    CHECK(clientPeer != 0);

    std::vector<coop::NetEvent> serverEvents;
    std::vector<coop::NetEvent> clientEvents;
    uint32_t serverPeer = 0;
    bool gotReliable = false;
    bool gotStream = false;
    for (int i = 0; i < 500 && !(gotReliable && gotStream); i++) {
        serverEvents.clear();
        clientEvents.clear();
        server.Service(1, serverEvents);
        client.Service(1, clientEvents);
        for (auto& e : clientEvents) {
            if (e.type == coop::NetEvent::Connect) {
                CHECK_EQ(e.peer, clientPeer);
                client.Send(clientPeer, 0, "hi", 2);
                client.Send(clientPeer, 1, "st", 2);
            }
        }
        for (auto& e : serverEvents) {
            if (e.type == coop::NetEvent::Connect) {
                serverPeer = e.peer;
            }
            if (e.type == coop::NetEvent::Receive && e.channel == 0) {
                gotReliable = std::string(e.data.begin(), e.data.end()) == "hi";
            }
            if (e.type == coop::NetEvent::Receive && e.channel == 1) {
                gotStream = std::string(e.data.begin(), e.data.end()) == "st";
            }
        }
    }
    CHECK(serverPeer != 0);
    CHECK(gotReliable);
    CHECK(gotStream);
    CHECK_EQ(server.PeerIp(serverPeer), std::string("127.0.0.1"));
}

TEST_CASE(TransportDisconnectReachesServer) {
    coop::Transport server;
    std::string err;
    CHECK(server.Listen(47701, 4, &err));
    bool serverSawDisconnect = false;
    {
        coop::Transport client;
        uint32_t peer = 0;
        CHECK(client.Connect("127.0.0.1", 47701, &peer, &err));
        std::vector<coop::NetEvent> ev;
        bool connected = false;
        for (int i = 0; i < 500 && !connected; i++) {
            ev.clear();
            server.Service(1, ev);
            ev.clear();
            client.Service(1, ev);
            for (auto& e : ev) {
                connected = connected || e.type == coop::NetEvent::Connect;
            }
        }
        CHECK(connected);
        client.Disconnect(peer);
        for (int i = 0; i < 200; i++) {
            ev.clear();
            client.Service(1, ev);
        }
    }
    std::vector<coop::NetEvent> ev;
    for (int i = 0; i < 500 && !serverSawDisconnect; i++) {
        ev.clear();
        server.Service(1, ev);
        for (auto& e : ev) {
            serverSawDisconnect = serverSawDisconnect || e.type == coop::NetEvent::Disconnect;
        }
    }
    CHECK(serverSawDisconnect);
}

TEST_CASE(TransportFilesChannelIsReliableAndOrdered) {
    coop::Transport server;
    coop::Transport client;
    std::string err;
    CHECK(server.Listen(47710, 4, &err));
    uint32_t clientPeer = 0;
    CHECK(client.Connect("127.0.0.1", 47710, &clientPeer, &err));
    uint32_t serverPeer = 0;
    std::vector<int> got;
    std::vector<uint8_t> packet(16000);
    for (int i = 0; i < 3000 && got.size() < 100; i++) {
        std::vector<coop::NetEvent> serverEvents;
        std::vector<coop::NetEvent> clientEvents;
        server.Service(1, serverEvents);
        client.Service(1, clientEvents);
        for (auto& e : serverEvents) {
            if (e.type == coop::NetEvent::Connect) {
                serverPeer = e.peer;
                for (int n = 0; n < 100; n++) { // 1.6 MB at once: none may be lost or reordered
                    packet[0] = (uint8_t)n;
                    server.Send(serverPeer, coop::kChannelFiles, packet.data(), packet.size());
                }
            }
        }
        for (auto& e : clientEvents) {
            if (e.type == coop::NetEvent::Receive) {
                CHECK_EQ(e.channel, (uint8_t)coop::kChannelFiles);
                CHECK_EQ(e.data.size(), (size_t)16000);
                got.push_back(e.data[0]);
            }
        }
    }
    CHECK_EQ(got.size(), (size_t)100);
    for (int n = 0; n < (int)got.size(); n++) {
        CHECK_EQ(got[n], n);
    }
}

TEST_CASE(TransportListenTwiceOnSamePortFails) {
    coop::Transport a;
    coop::Transport b;
    std::string err;
    CHECK(a.Listen(47702, 4, &err));
    CHECK(!b.Listen(47702, 4, &err));
    CHECK(!err.empty());
}

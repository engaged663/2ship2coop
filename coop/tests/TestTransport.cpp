#include "TestMain.h"

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

TEST_CASE(TransportListenTwiceOnSamePortFails) {
    coop::Transport a;
    coop::Transport b;
    std::string err;
    CHECK(a.Listen(47702, 4, &err));
    CHECK(!b.Listen(47702, 4, &err));
    CHECK(!err.empty());
}

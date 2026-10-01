// Handshake ("hello" -> "welcome"/"reject") and the "leave" broadcast.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

// The executable every game in the world must share (sub-project D3): the first accepted one's, forgotten when
// nobody is left. Games that send no build (bots, tools) are not checked.
std::string sExpectedBuild;

bool BuildAccepted(Server& server, RemoteClient& client, const std::string& build) {
    if (!server.Config().requireSameBuild || build.empty()) {
        return true;
    }
    if (server.Players().WelcomedAll().empty()) {
        sExpectedBuild.clear();
    }
    if (sExpectedBuild.empty()) {
        sExpectedBuild = build;
        server.Log().Info(Tr(Msg::HostBuildLog, { build }));
        return true;
    }
    if (build == sExpectedBuild) {
        return true;
    }
    server.Reject(client, Tr(Msg::RejectBuild));
    return false;
}

json PlayerSummary(const RemoteClient& c) {
    return { { "id", c.id }, { "nick", c.nick }, { "scene", c.scene }, { "sceneName", c.sceneName } };
}

// The server's own headless game (sub-project D): it must show the server's secret. It is not a player: no slot,
// no join/leave messages, not in /list.
void WelcomeHost(Server& server, RemoteClient& client, const std::string& token) {
    const std::string& expected = server.Config().hostToken;
    if (expected.empty() || token != expected) {
        return server.Reject(client, Tr(Msg::RejectHostToken));
    }
    if ((int)server.Players().WelcomedHosts().size() >= kMaxHosts) {
        return server.Reject(client, Tr(Msg::RejectTooManyHosts));
    }
    client.id = server.Players().AllocateId();
    client.nick = kHostNick;
    client.host = true;
    client.welcomed = true;
    client.actorBudget = TokenBucket(kHostActorStreamBurst, kHostActorStreamPerSecond); // whole scenes
    json players = json::array(); // the players (never the other hosts): the enemies it simulates go for them
    for (RemoteClient* other : server.Players().Welcomed()) {
        players.push_back(PlayerSummary(*other));
    }
    json welcome = MakeEvent(ev::kWelcome);
    welcome["id"] = client.id;
    welcome["nick"] = client.nick;
    welcome["host"] = true;
    welcome["motd"] = "";
    welcome["players"] = players;
    server.SendEvent(client, welcome);
    server.Log().Info(Tr(Msg::HostConnected, { std::to_string(client.id) }));
}

void OnHello(Server& server, RemoteClient& client, const json& ev) {
    if (client.welcomed) {
        return; // a second hello is ignored
    }
    int64_t proto = GetInt(ev, "proto", -1);
    std::string nick = GetString(ev, "nick");
    std::string pass = GetString(ev, "pass");

    if (proto != (int64_t)kProtocolVersion) {
        return server.Reject(client, Tr(Msg::RejectProto, { std::to_string(kProtocolVersion), std::to_string(proto) }));
    }
    if (!BuildAccepted(server, client, GetString(ev, "build"))) {
        return;
    }
    if (GetBool(ev, "host")) {
        return WelcomeHost(server, client, GetString(ev, "token"));
    }
    if (server.Players().WelcomedCount() >= server.Config().maxPlayers) {
        return server.Reject(client, Tr(Msg::RejectFull, { std::to_string(server.Config().maxPlayers) }));
    }
    if (!IsValidNick(nick)) {
        return server.Reject(client, Tr(Msg::RejectBadNick));
    }
    if (server.Players().ByNick(nick) != nullptr) {
        return server.Reject(client, Tr(Msg::RejectNickTaken, { nick }));
    }
    std::string banReason;
    if (server.Access().IsBanned(nick, client.ip, &banReason)) {
        return server.Reject(client, banReason.empty() ? Tr(Msg::RejectBanned) : Tr(Msg::RejectBannedReason, { banReason }));
    }
    if (!server.Config().password.empty() && pass != server.Config().password) {
        return server.Reject(client, Tr(Msg::RejectPassword));
    }

    client.id = server.Players().AllocateId();
    client.nick = nick;
    client.welcomed = true;

    json players = json::array();
    for (RemoteClient* other : server.Players().Welcomed()) {
        if (other != &client) {
            players.push_back(PlayerSummary(*other));
        }
    }
    json welcome = MakeEvent(ev::kWelcome);
    welcome["id"] = client.id;
    welcome["nick"] = client.nick;
    welcome["motd"] = server.Config().motd.empty() ? Tr(Msg::DefaultMotd) : server.Config().motd;
    welcome["players"] = players;
    server.SendEvent(client, welcome);

    json join = MakeEvent(ev::kJoin);
    join["id"] = client.id;
    join["nick"] = client.nick;
    server.Broadcast(join, &client);
    server.Log().Info(Tr(Msg::PlayerJoined, { client.nick, client.ip, std::to_string(client.id) }));
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (!client.welcomed) {
        return;
    }
    if (client.host) {
        server.Log().Info(Tr(Msg::HostDisconnected, { std::to_string(client.id) }));
        return;
    }
    std::string reason = client.leaveReason.empty() ? Tr(Msg::LeaveDisconnected) : client.leaveReason;
    json leave = MakeEvent(ev::kLeave);
    leave["id"] = client.id;
    leave["nick"] = client.nick;
    leave["reason"] = reason;
    server.Broadcast(leave, &client);
    server.Log().Info(Tr(Msg::PlayerLeft, { client.nick, reason }));
}

} // namespace

COOP_SERVER_EVENT(sessionHello, ev::kHello, false, OnHello);
COOP_SERVER_ON_DISCONNECT(sessionLeave, OnDisconnect);

} // namespace coop::server

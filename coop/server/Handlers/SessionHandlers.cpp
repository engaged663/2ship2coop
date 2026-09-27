// Handshake ("hello" -> "welcome"/"reject") and the "leave" broadcast.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

json PlayerSummary(const RemoteClient& c) {
    return { { "id", c.id }, { "nick", c.nick }, { "scene", c.scene }, { "sceneName", c.sceneName } };
}

// The server's own headless game (sub-project D): it must show the server's secret. It is not a player: no slot,
// no join/leave messages, not in /list.
void WelcomeHost(Server& server, RemoteClient& client, const std::string& token) {
    const std::string& expected = server.Config().hostToken;
    if (expected.empty() || token != expected) {
        return server.Reject(client, "Este servidor no acepta anfitriones con esa clave.");
    }
    if ((int)server.Players().WelcomedHosts().size() >= kMaxHosts) {
        return server.Reject(client, "Ya hay demasiados anfitriones conectados.");
    }
    client.id = server.Players().AllocateId();
    client.nick = kHostNick;
    client.host = true;
    client.welcomed = true;
    json welcome = MakeEvent(ev::kWelcome);
    welcome["id"] = client.id;
    welcome["nick"] = client.nick;
    welcome["host"] = true;
    welcome["motd"] = "";
    welcome["players"] = json::array();
    server.SendEvent(client, welcome);
    server.Log().Info("Anfitrión conectado (id " + std::to_string(client.id) + ")");
}

void OnHello(Server& server, RemoteClient& client, const json& ev) {
    if (client.welcomed) {
        return; // a second hello is ignored
    }
    int64_t proto = GetInt(ev, "proto", -1);
    std::string nick = GetString(ev, "nick");
    std::string pass = GetString(ev, "pass");

    if (proto != (int64_t)kProtocolVersion) {
        return server.Reject(client, "Versión incompatible: el servidor usa el protocolo v" +
                                         std::to_string(kProtocolVersion) + " y tu juego el v" +
                                         std::to_string(proto) + ". Usad la misma versión del mod.");
    }
    if (GetBool(ev, "host")) {
        return WelcomeHost(server, client, GetString(ev, "token"));
    }
    if (server.Players().WelcomedCount() >= server.Config().maxPlayers) {
        return server.Reject(client, "El servidor está lleno (" + std::to_string(server.Config().maxPlayers) +
                                         " jugadores).");
    }
    if (!IsValidNick(nick)) {
        return server.Reject(client, "Nick inválido: usa de 3 a 16 letras, números o _.");
    }
    if (server.Players().ByNick(nick) != nullptr) {
        return server.Reject(client, "Ya hay alguien conectado con el nick '" + nick + "'.");
    }
    std::string banReason;
    if (server.Access().IsBanned(nick, client.ip, &banReason)) {
        return server.Reject(client, banReason.empty() ? "Estás baneado de este servidor."
                                                       : "Estás baneado de este servidor: " + banReason);
    }
    if (!server.Config().password.empty() && pass != server.Config().password) {
        return server.Reject(client, "Contraseña incorrecta.");
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
    welcome["motd"] = server.Config().motd;
    welcome["players"] = players;
    server.SendEvent(client, welcome);

    json join = MakeEvent(ev::kJoin);
    join["id"] = client.id;
    join["nick"] = client.nick;
    server.Broadcast(join, &client);
    server.Log().Info(client.nick + " se ha unido (" + client.ip + ", id " + std::to_string(client.id) + ")");
}

void OnDisconnect(Server& server, RemoteClient& client) {
    if (!client.welcomed) {
        return;
    }
    if (client.host) {
        server.Log().Info("Anfitrión desconectado (id " + std::to_string(client.id) + ")");
        return;
    }
    std::string reason = client.leaveReason.empty() ? "desconectado" : client.leaveReason;
    json leave = MakeEvent(ev::kLeave);
    leave["id"] = client.id;
    leave["nick"] = client.nick;
    leave["reason"] = reason;
    server.Broadcast(leave, &client);
    server.Log().Info(client.nick + " ha salido (" + reason + ")");
}

} // namespace

COOP_SERVER_EVENT(sessionHello, ev::kHello, false, OnHello);
COOP_SERVER_ON_DISCONNECT(sessionLeave, OnDisconnect);

} // namespace coop::server

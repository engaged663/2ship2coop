// Handshake ("hello" -> "welcome"/"reject") and the "leave" broadcast.
#include "server/Registry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

json PlayerSummary(const RemoteClient& c) {
    return { { "id", c.id }, { "nick", c.nick }, { "scene", c.scene }, { "sceneName", c.sceneName } };
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

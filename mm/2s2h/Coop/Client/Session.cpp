#include "Session.h"

#include "Dispatcher.h"
#include "NetClient.h"

#include "common/Protocol.h"
#include "common/Text.h"

#include "2s2h/BenGui/Notification.h"

#include <libultraship/bridge/consolevariablebridge.h>

namespace coop::client {

namespace {

bool sConnected = false;
uint8_t sLocalId = 0;
std::string sLocalNick;
std::map<uint8_t, RemotePlayer> sPlayers;

void Notify(const std::string& message, ImVec4 color) {
    Notification::Emit({ .prefix = "Co-op", .message = message, .messageColor = color, .remainingTime = 6.0f });
}

void OnWelcome(const json& ev) {
    NetClient::Get().MarkWelcomed();
    sConnected = true;
    sLocalId = (uint8_t)GetInt(ev, "id");
    sLocalNick = GetString(ev, "nick");
    sPlayers.clear();
    auto players = ev.find("players");
    if (players != ev.end() && players->is_array()) {
        for (const json& p : *players) {
            RemotePlayer rp;
            rp.id = (uint8_t)GetInt(p, "id");
            rp.nick = GetString(p, "nick");
            rp.scene = (int16_t)GetInt(p, "scene", -1);
            rp.sceneName = GetString(p, "sceneName");
            sPlayers[rp.id] = rp;
        }
    }
    Notify("Conectado a " + NetClient::Get().ServerLabel() + " como " + sLocalNick, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
}

void OnJoin(const json& ev) {
    RemotePlayer rp;
    rp.id = (uint8_t)GetInt(ev, "id");
    rp.nick = GetString(ev, "nick");
    sPlayers[rp.id] = rp;
}

void OnLeave(const json& ev) {
    sPlayers.erase((uint8_t)GetInt(ev, "id"));
}

void OnLoc(const json& ev) {
    auto it = sPlayers.find((uint8_t)GetInt(ev, "id"));
    if (it != sPlayers.end()) {
        it->second.scene = (int16_t)GetInt(ev, "scene", -1);
        it->second.sceneName = GetString(ev, "sceneName");
    }
}

void OnLost(const std::string& reason) {
    sConnected = false;
    sLocalId = 0;
    sPlayers.clear();
    Notify(reason, ImVec4(1.0f, 0.6f, 0.4f, 1.0f));
}

} // namespace

bool Session_IsConnected() {
    return sConnected;
}

uint8_t Session_LocalId() {
    return sLocalId;
}

const std::string& Session_LocalNick() {
    return sLocalNick;
}

const std::map<uint8_t, RemotePlayer>& Session_Players() {
    return sPlayers;
}

const RemotePlayer* Session_FindPlayer(uint8_t id) {
    auto it = sPlayers.find(id);
    return it != sPlayers.end() ? &it->second : nullptr;
}

bool Session_ConnectFromSettings() {
    std::string nick = CVarGetString("gCoop.Nick", "");
    std::string host = CVarGetString("gCoop.Host", "127.0.0.1");
    int port = CVarGetInteger("gCoop.Port", kDefaultPort);
    std::string pass = CVarGetString("gCoop.Password", "");
    if (!IsValidNick(nick)) {
        Notify("Elige un nick válido (3-16 letras, números o _) en el menú Co-op.", ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        return false;
    }
    if (host.empty() || port <= 0 || port > 65535) {
        Notify("Revisa la IP y el puerto del servidor en el menú Co-op.", ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        return false;
    }
    NetClient::Get().Connect(host, (uint16_t)port, nick, pass);
    return true;
}

COOP_ON_EVENT(sessionWelcome, ev::kWelcome, OnWelcome);
COOP_ON_EVENT(sessionJoin, ev::kJoin, OnJoin);
COOP_ON_EVENT(sessionLeave, ev::kLeave, OnLeave);
COOP_ON_EVENT(sessionLoc, ev::kLoc, OnLoc);
COOP_ON_LOST(sessionLost, OnLost);

} // namespace coop::client

#include "Session.h"

#include "Dispatcher.h"
#include "NetClient.h"

#include "2s2h/Coop/Features/PlayerColors.h"
#include "2s2h/Coop/I18n/CoopText.h"

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
int64_t sLocalColor = -1;  // ours (Features/PlayerColors.cpp keeps it as the menu has it)
int64_t sServerColor = -1; // what hello / "color" told the server

void SendColorIfChanged() {
    if (sConnected && sLocalColor != sServerColor) {
        sServerColor = sLocalColor;
        json ev = MakeEvent(ev::kColor);
        ev["color"] = sLocalColor;
        NetClient::Get().SendEvent(ev);
    }
}

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
            rp.color = GetInt(p, "color", -1);
            sPlayers[rp.id] = rp;
        }
    }
    Notify(Tx(Txt::SessionConnected, { NetClient::Get().ServerLabel(), sLocalNick }), ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
    SendColorIfChanged(); // changed in the menu while the hello was on its way
}

void OnJoin(const json& ev) {
    RemotePlayer rp;
    rp.id = (uint8_t)GetInt(ev, "id");
    rp.nick = GetString(ev, "nick");
    rp.color = GetInt(ev, "color", -1);
    sPlayers[rp.id] = rp;
}

void OnColor(const json& ev) {
    auto it = sPlayers.find((uint8_t)GetInt(ev, "id"));
    if (it != sPlayers.end()) {
        it->second.color = GetInt(ev, "color", -1);
    }
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
        Notify(Tx(Txt::SessionBadNick), ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        return false;
    }
    if (host.empty() || port <= 0 || port > 65535) {
        Notify(Tx(Txt::SessionBadHost), ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        return false;
    }
    sLocalColor = PlayerColors_SettingsPacked();
    sServerColor = sLocalColor; // the hello carries it
    NetClient::Get().Connect(host, (uint16_t)port, nick, pass, sLocalColor);
    return true;
}

int64_t Session_LocalColor() {
    return sLocalColor;
}

void Session_SetLocalColor(int64_t color) {
    sLocalColor = color;
    SendColorIfChanged();
}

COOP_ON_EVENT(sessionWelcome, ev::kWelcome, OnWelcome);
COOP_ON_EVENT(sessionJoin, ev::kJoin, OnJoin);
COOP_ON_EVENT(sessionLeave, ev::kLeave, OnLeave);
COOP_ON_EVENT(sessionLoc, ev::kLoc, OnLoc);
COOP_ON_EVENT(sessionColor, ev::kColor, OnColor);
COOP_ON_LOST(sessionLost, OnLost);

} // namespace coop::client

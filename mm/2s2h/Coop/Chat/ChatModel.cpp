#include "ChatModel.h"

#include "2s2h/Coop/Client/Dispatcher.h"
#include "2s2h/Coop/Client/NetClient.h"
#include "2s2h/Coop/Client/Session.h"

#include "common/Protocol.h"

#include <chrono>

namespace coop::client {

namespace {

constexpr size_t kMaxLines = 100;
constexpr size_t kMaxSentHistory = 20;

std::deque<ChatLine> sLines;
std::vector<std::string> sSent;

ChatKind KindForLevel(const std::string& level) {
    if (level == level::kOk) {
        return ChatKind::Ok;
    }
    if (level == level::kWarn) {
        return ChatKind::Warn;
    }
    if (level == level::kError) {
        return ChatKind::Error;
    }
    return ChatKind::Info;
}

void OnChat(const json& ev) {
    Chat_Add(ChatKind::Chat, "<" + GetString(ev, "from") + "> " + GetString(ev, "text"));
}

void OnPm(const json& ev) {
    std::string from = GetString(ev, "from");
    std::string to = GetString(ev, "to");
    std::string text = GetString(ev, "text");
    if (from == Session_LocalNick()) {
        Chat_Add(ChatKind::Private, "[tú -> " + to + "] " + text);
    } else {
        Chat_Add(ChatKind::Private, "[" + from + " -> tú] " + text);
    }
}

void OnSys(const json& ev) {
    Chat_Add(KindForLevel(GetString(ev, "level")), GetString(ev, "text"));
}

void OnWelcome(const json& ev) {
    std::string motd = GetString(ev, "motd");
    if (!motd.empty()) {
        Chat_Add(ChatKind::Info, motd);
    }
}

void OnJoin(const json& ev) {
    Chat_Add(ChatKind::Presence, GetString(ev, "nick") + " se ha unido a la partida.");
}

void OnLeave(const json& ev) {
    std::string reason = GetString(ev, "reason");
    std::string text = GetString(ev, "nick") + " ha salido de la partida";
    text += (reason.empty() || reason == "desconectado") ? "." : " (" + reason + ").";
    Chat_Add(ChatKind::Presence, text);
}

void OnLost(const std::string& reason) {
    Chat_Add(ChatKind::Warn, reason);
}

} // namespace

double Chat_Now() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void Chat_Add(ChatKind kind, const std::string& text) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty()) {
            sLines.push_back({ kind, line, Chat_Now() });
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    while (sLines.size() > kMaxLines) {
        sLines.pop_front();
    }
}

const std::deque<ChatLine>& Chat_Lines() {
    return sLines;
}

void Chat_Submit(const std::string& text) {
    if (text.empty()) {
        return;
    }
    if (sSent.empty() || sSent.back() != text) {
        sSent.push_back(text);
        if (sSent.size() > kMaxSentHistory) {
            sSent.erase(sSent.begin());
        }
    }
    if (!Session_IsConnected()) {
        Chat_Add(ChatKind::Error, "No estás conectado a ningún servidor.");
        return;
    }
    json ev = MakeEvent(text[0] == '/' ? ev::kCmd : ev::kChat);
    ev[text[0] == '/' ? "line" : "text"] = text;
    NetClient::Get().SendEvent(ev);
}

const std::vector<std::string>& Chat_SentHistory() {
    return sSent;
}

COOP_ON_EVENT(chatChat, ev::kChat, OnChat);
COOP_ON_EVENT(chatPm, ev::kPm, OnPm);
COOP_ON_EVENT(chatSys, ev::kSys, OnSys);
COOP_ON_EVENT(chatWelcome, ev::kWelcome, OnWelcome);
COOP_ON_EVENT(chatJoin, ev::kJoin, OnJoin);
COOP_ON_EVENT(chatLeave, ev::kLeave, OnLeave);
COOP_ON_LOST(chatLost, OnLost);

} // namespace coop::client

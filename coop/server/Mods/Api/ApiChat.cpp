// Mod API: chat.* (what the players read in their chat window).
#include "server/Mods/ModApi.h"
#include "server/Mods/ModHost.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

constexpr size_t kMaxLines = 8; // of one system text (the chat window shows each line break as a new line)

// A text of up to kMaxLines chat lines; empty is an error.
std::string SystemText(ApiCall& call, size_t i) {
    std::string text = SanitizeText(call.Str(i, "text"), (size_t)kChatMaxChars * kMaxLines);
    if (text.empty()) {
        call.Fail(Tr(Msg::ApiArgEmpty, { std::to_string(i + 1), "text" }));
    }
    return text;
}

json ChatBroadcast(ApiCall& call) {
    std::string text = SystemText(call, 0);
    const char* level = call.Level(1);
    for (RemoteClient* p : call.server.Players().Welcomed()) {
        call.server.SendSystem(p, text, level);
    }
    call.host.Log(call.mod, "info", text);
    return nullptr;
}

json ChatTell(ApiCall& call) {
    RemoteClient& to = call.Player(0);
    call.server.SendSystem(&to, SystemText(call, 1), call.Level(2));
    return nullptr;
}

json ChatSay(ApiCall& call) {
    std::string name = SanitizeChat(call.Str(0, "name", 200), 24);
    std::string text = SanitizeChat(call.Str(1, "text"), kChatMaxChars);
    if (name.empty()) {
        call.Fail(Tr(Msg::ApiArgEmpty, { "1", "name" }));
    }
    if (text.empty()) {
        call.Fail(Tr(Msg::ApiArgEmpty, { "2", "text" }));
    }
    json out = MakeEvent(ev::kChat);
    out["from"] = name;
    out["text"] = text;
    call.server.Broadcast(out);
    call.server.Log().Info("<" + name + "> " + text);
    return nullptr;
}

} // namespace

COOP_MOD_API(chatBroadcast, "chat.broadcast", "text, level?", "nada",
             "Escribe una línea de sistema en el chat de todos los jugadores conectados (y en el registro). `level` "
             "le da color: `info` (por defecto), `ok`, `warn` o `error`. El texto puede tener hasta 8 líneas "
             "separadas por `\\n`.",
             ChatBroadcast);
COOP_MOD_API(chatTell, "chat.tell", "player, text, level?", "nada",
             "Escribe una línea de sistema solo en el chat de ese jugador.", ChatTell);
COOP_MOD_API(chatSay, "chat.say", "name, text", "nada",
             "Escribe en el chat de todos una línea normal, como si la dijera `name` (un personaje, un bot): "
             "`<Tatl> ¡Escucha!`. `name` no tiene que ser un jugador.",
             ChatSay);

} // namespace coop::server

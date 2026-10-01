// Public chat ("chat") and slash commands typed in the game ("cmd").
#include "server/CommandRegistry.h"
#include "server/Registry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void OnChat(Server& server, RemoteClient& client, const json& ev) {
    if (!server.AllowMessage(client)) {
        return;
    }
    std::string text = SanitizeChat(GetString(ev, "text"), kChatMaxChars);
    if (text.empty()) {
        return;
    }
    json out = MakeEvent(ev::kChat);
    out["from"] = client.nick;
    out["text"] = text;
    server.Broadcast(out); // the sender sees its own line only once the server accepted it
    server.Log().Info("<" + client.nick + "> " + text);
}

void OnCommand(Server& server, RemoteClient& client, const json& ev) {
    if (!server.AllowMessage(client)) {
        return;
    }
    std::string line = GetString(ev, "line");
    if (line.size() > (size_t)kCommandMaxChars) {
        server.SendSystem(&client, Tr(Msg::CommandTooLong), level::kError);
        return;
    }
    ExecuteCommandLine(server, &client, line);
}

} // namespace

COOP_SERVER_EVENT(chatChat, ev::kChat, true, OnChat);
COOP_SERVER_EVENT(chatCommand, ev::kCmd, true, OnCommand);

} // namespace coop::server

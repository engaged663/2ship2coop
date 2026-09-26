// /list: who is online and where.
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    auto players = ctx.server.Players().Welcomed();
    std::string text = "Jugadores (" + std::to_string(players.size()) + "/" +
                       std::to_string(ctx.server.Config().maxPlayers) + "):";
    for (RemoteClient* p : players) {
        text += "\n" + p->nick;
        if (!p->sceneName.empty()) {
            text += " - " + p->sceneName;
        }
        if (ctx.server.IsOp(*p)) {
            text += " [admin]";
        }
    }
    ctx.Reply(text);
}

COOP_COMMAND(list, "list", "/list", "jugadores conectados y dónde están", Perm::Player, 0, Run);

} // namespace coop::server

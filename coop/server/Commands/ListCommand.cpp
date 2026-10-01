// /list: who is online and where.
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    auto players = ctx.server.Players().Welcomed();
    std::string text =
        Tr(Msg::ListTitle, { std::to_string(players.size()), std::to_string(ctx.server.Config().maxPlayers) });
    for (RemoteClient* p : players) {
        text += "\n" + p->nick;
        if (!p->sceneName.empty()) {
            text += " - " + p->sceneName;
        }
        if (p->inWorld && !p->activityName.empty()) {
            text += Tr(Msg::ListActivityTag, { p->activityName });
        }
        if (ctx.server.IsOp(*p)) {
            text += Tr(Msg::ListAdminTag);
        }
    }
    ctx.Reply(text);
}

COOP_COMMAND(list, "list", Msg::ListUsage, Msg::ListHelp, Perm::Player, 0, Run);

} // namespace coop::server

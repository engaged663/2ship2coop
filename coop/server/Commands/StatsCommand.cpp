// /stats: per-player network counters (diagnostics for the server owner).
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = Tr(Msg::StatsTitle);
    for (RemoteClient* p : ctx.server.Players().Welcomed()) {
        text += "\n" + Tr(Msg::StatsLine, { p->nick, std::to_string(p->scene), std::to_string(p->room),
                                           std::to_string(p->streamsIn), std::to_string(p->streamsRelayed) });
    }
    ctx.Reply(text);
}

COOP_COMMAND(stats, "stats", Msg::StatsUsage, Msg::StatsHelp, Perm::Op, 0, Run);

} // namespace coop::server

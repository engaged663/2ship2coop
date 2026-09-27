// /stats: per-player network counters (diagnostics for the server owner).
#include "server/CommandRegistry.h"
#include "server/Server.h"

namespace coop::server {

static void Run(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = "Estadísticas de red:";
    for (RemoteClient* p : ctx.server.Players().Welcomed()) {
        text += "\n" + p->nick + ": escena " + std::to_string(p->scene) + ", sala " + std::to_string(p->room) +
                ", poses recibidas " + std::to_string(p->streamsIn) + ", reenviadas " +
                std::to_string(p->streamsRelayed);
    }
    ctx.Reply(text);
}

COOP_COMMAND(stats, "stats", "/stats", "estadísticas de red por jugador", Perm::Op, 0, Run);

} // namespace coop::server

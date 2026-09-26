// /say and /stop.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Say(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string text = SanitizeChat(JoinFrom(args, 0), kChatMaxChars);
    if (text.empty()) {
        return;
    }
    for (RemoteClient* p : ctx.server.Players().Welcomed()) {
        ctx.server.SendSystem(p, "[Servidor] " + text, level::kInfo);
    }
    ctx.server.Log().Info("[Servidor] " + text);
}

void Stop(CommandContext& ctx, const std::vector<std::string>& args) {
    ctx.server.Stop(args.empty() ? "cerrado desde la consola" : JoinFrom(args, 0));
}

} // namespace

COOP_COMMAND(say, "say", "/say <mensaje>", "anuncio del servidor para todos", Perm::Op, 1, Say);
COOP_COMMAND(stop, "stop", "/stop [motivo]", "cerrar el servidor", Perm::Console, 0, Stop);

} // namespace coop::server

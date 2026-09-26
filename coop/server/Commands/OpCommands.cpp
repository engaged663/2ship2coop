// /op and /deop (console only): ops may use /kick, /ban, /unban, /banlist and /say from the game chat.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Op(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!IsValidNick(args[0])) {
        ctx.Reply("Nick inválido: '" + args[0] + "'.", level::kError);
        return;
    }
    if (!ctx.server.Access().AddOp(args[0])) {
        ctx.Reply(args[0] + " ya era administrador.", level::kWarn);
        return;
    }
    ctx.Reply(args[0] + " ahora es administrador.", level::kOk);
    if (RemoteClient* target = ctx.server.Players().ByNick(args[0])) {
        ctx.server.SendSystem(target, "Ahora eres administrador del servidor.", level::kOk);
    }
}

void Deop(CommandContext& ctx, const std::vector<std::string>& args) {
    if (!ctx.server.Access().RemoveOp(args[0])) {
        ctx.Reply(args[0] + " no era administrador.", level::kWarn);
        return;
    }
    ctx.Reply(args[0] + " ya no es administrador.", level::kOk);
    if (RemoteClient* target = ctx.server.Players().ByNick(args[0])) {
        ctx.server.SendSystem(target, "Ya no eres administrador del servidor.", level::kWarn);
    }
}

} // namespace

COOP_COMMAND(op, "op", "/op <jugador>", "dar permisos de administrador", Perm::Console, 1, Op);
COOP_COMMAND(deop, "deop", "/deop <jugador>", "quitar permisos de administrador", Perm::Console, 1, Deop);

} // namespace coop::server

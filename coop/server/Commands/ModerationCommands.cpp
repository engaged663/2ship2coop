// /kick, /ban, /unban, /banlist.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Text.h"

namespace coop::server {

namespace {

void Kick(CommandContext& ctx, const std::vector<std::string>& args) {
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target == nullptr) {
        ctx.Reply("No hay ningún jugador conectado llamado '" + args[0] + "'.", level::kError);
        return;
    }
    std::string nick = target->nick;
    ctx.server.Kick(*target, JoinFrom(args, 1));
    ctx.Reply(nick + " ha sido expulsado.", level::kOk);
}

void Ban(CommandContext& ctx, const std::vector<std::string>& args) {
    std::string reason = JoinFrom(args, 1);
    RemoteClient* target = ctx.server.Players().ByNick(args[0]);
    if (target != nullptr) {
        std::string nick = target->nick;
        ctx.server.Access().Ban(nick, target->ip, reason);
        ctx.server.Kick(*target, reason.empty() ? "baneado" : "baneado: " + reason);
        ctx.Reply(nick + " ha sido baneado (nick e IP).", level::kOk);
    } else {
        if (!IsValidNick(args[0])) {
            ctx.Reply("Nick inválido: '" + args[0] + "'.", level::kError);
            return;
        }
        ctx.server.Access().Ban(args[0], "", reason);
        ctx.Reply(args[0] + " no está conectado; se ha baneado su nick.", level::kOk);
    }
}

void Unban(CommandContext& ctx, const std::vector<std::string>& args) {
    if (ctx.server.Access().Unban(args[0])) {
        ctx.Reply("Se ha quitado el ban de '" + args[0] + "'.", level::kOk);
    } else {
        ctx.Reply("No había ningún ban para '" + args[0] + "'.", level::kError);
    }
}

void BanList(CommandContext& ctx, const std::vector<std::string>& args) {
    const auto& bans = ctx.server.Access().Bans();
    if (bans.empty()) {
        ctx.Reply("No hay nadie baneado.");
        return;
    }
    std::string text = "Baneados (" + std::to_string(bans.size()) + "):";
    for (const auto& b : bans) {
        text += "\n" + (b.nick.empty() ? std::string("-") : b.nick) + " " + (b.ip.empty() ? "" : "[" + b.ip + "] ") +
                b.date + (b.reason.empty() ? "" : " - " + b.reason);
    }
    ctx.Reply(text);
}

} // namespace

COOP_COMMAND(kick, "kick", "/kick <jugador> [motivo]", "expulsar a un jugador", Perm::Op, 1, Kick);
COOP_COMMAND(ban, "ban", "/ban <jugador> [motivo]", "banear nick e IP y expulsar", Perm::Op, 1, Ban);
COOP_COMMAND(unban, "unban", "/unban <nick|ip>", "quitar un ban", Perm::Op, 1, Unban);
COOP_COMMAND(banlist, "banlist", "/banlist", "lista de baneados", Perm::Op, 0, BanList);

} // namespace coop::server

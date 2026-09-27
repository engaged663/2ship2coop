// Shared world commands: /tiempo, /si, /no (Song of Time vote); for admins /settime, /mundo, /reiniciar.
#include "server/CommandRegistry.h"
#include "server/Server.h"

#include "common/Clock.h"
#include "common/Text.h"

namespace coop::server {

namespace {

void Time(CommandContext& ctx, const std::vector<std::string>&) {
    ctx.Reply(ctx.server.World().TimeText());
}

void CastVote(CommandContext& ctx, bool yes) {
    if (ctx.IsConsole()) {
        ctx.Reply("Solo votan los jugadores que están en la partida del servidor.", level::kError);
        return;
    }
    ctx.server.World().Vote(*ctx.sender, yes);
}

void VoteYes(CommandContext& ctx, const std::vector<std::string>&) {
    CastVote(ctx, true);
}

void VoteNo(CommandContext& ctx, const std::vector<std::string>&) {
    CastVote(ctx, false);
}

void SetTime(CommandContext& ctx, const std::vector<std::string>& args) {
    int day = 0;
    uint32_t abs = 0;
    if (!ParseInt(args[0], day) || !clock::Parse(day, args[1], abs)) {
        ctx.Reply("Uso: /settime <día 1-3> <hh:mm>, por ejemplo /settime 2 18:00", level::kError);
        return;
    }
    std::string err;
    if (!ctx.server.World().SetTime(abs, ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

void ShowWorld(CommandContext& ctx, const std::vector<std::string>&) {
    ctx.Reply(ctx.server.World().Describe());
}

void Restart(CommandContext& ctx, const std::vector<std::string>&) {
    std::string err;
    if (!ctx.server.World().Restart(ctx.SenderName(), &err)) {
        ctx.Reply(err, level::kError);
    }
}

} // namespace

COOP_COMMAND(tiempo, "tiempo", "/tiempo", "día y hora de la partida del servidor", Perm::Player, 0, Time);
COOP_COMMAND(si, "si", "/si", "votar sí a volver al Amanecer del Primer Día", Perm::Player, 0, VoteYes);
COOP_COMMAND(no, "no", "/no", "votar no a volver al Amanecer del Primer Día", Perm::Player, 0, VoteNo);
COOP_COMMAND(settime, "settime", "/settime <día 1-3> <hh:mm>", "cambiar el día y la hora para todos", Perm::Op, 2,
             SetTime);
COOP_COMMAND(mundo, "mundo", "/mundo", "estado de la partida del servidor", Perm::Op, 0, ShowWorld);
COOP_COMMAND(reiniciar, "reiniciar", "/reiniciar", "volver al Amanecer del Primer Día sin votación", Perm::Op, 0,
             Restart);

} // namespace coop::server
